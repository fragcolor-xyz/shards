#include "generator.hpp"
#include "fmt.hpp"
#include <shards/fast_string/fmt.hpp>
#include "log/log.hpp"
#include "shader/struct_layout.hpp"
#include "spdlog/logger.h"
#include "wgsl_mapping.hpp"
#include "type_builder.hpp"
#include "../enums.hpp"
#include "../log.hpp"
#include "../error_utils.hpp"
#include "../graph.hpp"
#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <magic_enum.hpp>
#include <optional>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

namespace gfx {
namespace shader {

static auto logger = getLogger();

static std::vector<const EntryPoint *> getEntryPointPtrs(const std::vector<EntryPoint> &_entryPoints) {
  std::vector<const EntryPoint *> entryPoints;
  std::transform(_entryPoints.begin(), _entryPoints.end(), std::back_inserter(entryPoints),
                 [](const EntryPoint &entryPoint) { return &entryPoint; });
  return entryPoints;
}

template <typename T>
static void generateTextureVars(T &output, const TextureDefinition &def, size_t group, size_t binding, size_t samplerBinding) {
  output += fmt::format("@group({}) @binding({})\n", group, binding);
  output += fmt::format("var {}: {};\n", def.variableName, getWGSLTypeName(def.type));

  SamplerType SamplerType{};
  output += fmt::format("@group({}) @binding({})\n", group, samplerBinding);
  output += fmt::format("var {}: {};\n", def.defaultSamplerVariableName, getWGSLTypeName(SamplerType));
}

// Pads a struct to array stride inside an array body
template <typename T> static void generatePaddingForArrayStruct(T &output, const StructLayout &layout) {
  size_t alignedSize = layout.getArrayStride();
  shassert(alignTo<4>(alignedSize) == alignedSize); // Check multiple of 4
  size_t size4ToPad = (alignedSize - layout.size) / 4;
  if (size4ToPad > 0) {
    output += fmt::format("\t_array_padding_: array<f32, {}>,\n", size4ToPad);
  }
}

// Generate buffer struct and binding definitions in shader source
template <typename T>
static void generateBuffer(T &output, TypeBuilder &tb, const std::string &name, const Type &type, AddressSpace addressSpace,
                           size_t group, size_t binding, const Dimension &dimension) {
  // String structName = name + "_t";
  // output += fmt::format("struct {} {{\n", structName);
  // for (size_t i = 0; i < layout.fieldNames.size(); i++) {
  //   output += fmt::format("\t{}: {},\n", sanitizeIdentifier(layout.fieldNames[i]), getWGSLTypeName(layout.items[i].type));
  // }

  // Only for buffers that use dynamic offset
  // if (type == AddressSpace::Storage)
  //   generatePaddingForArrayStruct(output, layout);

  // output += "};\n";

  String typeName = tb.resolveTypeName(type);

  // Container struct
  String containerTypeName = name + "_container";
  const char *varType = containerTypeName.c_str();

  std::visit(
      [&](auto &&arg) {
        using T1 = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T1, dim::One>) {
          varType = typeName.c_str();
        } else if constexpr (std::is_same_v<T1, dim::Fixed>) {
          output += fmt::format("struct {} {{ elements: array<{}, {}> }};\n", containerTypeName, typeName, arg.length);
        } else {
          output += fmt::format("struct {} {{ elements: array<{}> }};\n", containerTypeName, typeName);
        }
      },
      dimension);

  // global storage/uniform variable
  const char *varStorageType = nullptr;
  switch (addressSpace) {
  case AddressSpace::Uniform:
    varStorageType = "uniform";
    break;
  case AddressSpace::Storage:
    varStorageType = "storage";
    break;
  case AddressSpace::StorageRW:
    varStorageType = "storage, read_write";
    break;
  }
  output += fmt::format("@group({}) @binding({})\n", group, binding);
  output += fmt::format("var<{}> {}: {};\n", varStorageType, name, varType);
}

struct InternalStructField {
  static constexpr size_t NO_LOCATION = ~0;
  NamedNumType base;
  size_t location = NO_LOCATION;
  FastString builtinTag;

  InternalStructField() = default;
  InternalStructField(const NamedNumType &base) : base(base) {}
  InternalStructField(const NamedNumType &base, size_t location) : base(base), location(location) {}
  InternalStructField(const NamedNumType &base, FastString builtinTag) : base(base), builtinTag(builtinTag) {}
  bool hasLocation() const { return location != NO_LOCATION; }
  bool hasBuiltinTag() const { return !builtinTag.empty(); }
};

template <typename T>
static void generateStruct(T &output, const String &typeName, const std::vector<InternalStructField> &fields, bool interpolated = true) {
  output += fmt::format("struct {} {{\n", typeName);
  for (auto &field : fields) {
    std::string typeName = getWGSLTypeName(field.base.type);

    String extraTags;
    if (interpolated && isIntegerType(field.base.type.baseType)) {
      // integer vertex outputs requires flat interpolation
      extraTags = "@interpolate(flat)";
    }

    if (field.hasBuiltinTag()) {
      output += fmt::format("\t@builtin({}) {}", field.builtinTag, extraTags);
    } else if (field.hasLocation()) {
      output += fmt::format("\t@location({}) {}", field.location, extraTags);
    }
    output += fmt::format("{}: {},\n", sanitizeIdentifier(field.base.name), typeName);
  }
  output += "};\n";
}

static size_t getNextStructLocation(const std::vector<InternalStructField> &_struct) {
  size_t loc = 0;
  for (auto &field : _struct) {
    if (field.hasLocation()) {
      loc = std::max(loc, field.location + 1);
    }
  }
  return loc;
}

struct StageOutput {
  String code;
  std::vector<GeneratorError> errors;
};

static bool sortEntryPoints(std::vector<const EntryPoint *> &entryPoints, bool ignoreMissingDependencies = true) {
  std::unordered_map<FastString, size_t> nodeNames;
  for (size_t i = 0; i < entryPoints.size(); i++) {
    const EntryPoint &entryPoint = *entryPoints[i];
    if (!entryPoint.name.empty())
      nodeNames.insert_or_assign(entryPoint.name, i);
  }

  auto resolveNodeIndex = [&](FastString name) -> const size_t * {
    auto it = nodeNames.find(name);
    if (it != nodeNames.end())
      return &it->second;
    return nullptr;
  };

  std::set<FastString> missingDependencies;
  graph::Graph graph;
  graph.nodes.resize(entryPoints.size());
  for (size_t i = 0; i < entryPoints.size(); i++) {
    const EntryPoint &entryPoint = *entryPoints[i];
    for (auto &dep : entryPoint.dependencies) {
      if (dep.type == DependencyType::Before) {
        if (const size_t *depIndex = resolveNodeIndex(dep.name)) {
          graph.nodes[*depIndex].dependencies.push_back(i);
        } else if (!ignoreMissingDependencies) {
          missingDependencies.insert(dep.name);
        }
      } else {
        if (const size_t *depIndex = resolveNodeIndex(dep.name)) {
          graph.nodes[i].dependencies.push_back(*depIndex);
        } else if (!ignoreMissingDependencies) {
          missingDependencies.insert(dep.name);
        }
      }
    }
  }

  if (!ignoreMissingDependencies && missingDependencies.size() > 0) {
    return false;
  }

  std::vector<size_t> sortedIndices;
  if (!graph::topologicalSort(graph, sortedIndices))
    return false;

  auto unsortedEntryPoints = std::move(entryPoints);
  entryPoints.clear();
  for (size_t i = 0; i < sortedIndices.size(); i++) {
    entryPoints.push_back(unsortedEntryPoints[sortedIndices[i]]);
  }
  return true;
}

struct DynamicVertexInput : public IGeneratorDynamicHandler {
  std::vector<InternalStructField> &inputStruct;

  DynamicVertexInput(std::vector<InternalStructField> &inputStruct) : inputStruct(inputStruct) {}

  bool createDynamicInput(FastString name, NumType &out) {
    if (name == "vertex_index") {
      out = NumType(ShaderFieldBaseType::UInt32);
      InternalStructField newField = generateDynamicStructInput(name, out);
      inputStruct.push_back(newField);
      return true;
    }
    return false;
  }

  InternalStructField generateDynamicStructInput(FastString name, const NumType &type) {
    if (name == "vertex_index") {
      return InternalStructField(NamedNumType(name, type), "vertex_index");
    } else {
      throw std::logic_error("Unknown dynamic vertex input");
    }
  }
};

struct DynamicVertexOutput : public IGeneratorDynamicHandler {
  std::vector<InternalStructField> &outputStruct;

  DynamicVertexOutput(std::vector<InternalStructField> &outputStruct) : outputStruct(outputStruct) {}

  bool createDynamicOutput(FastString name, NumType requestedType) {
    InternalStructField newField = generateDynamicStructOutput(name, requestedType);
    outputStruct.push_back(newField);
    return true;
  }

  InternalStructField generateDynamicStructOutput(FastString name, const NumType &type) {
    // Handle builtin outputs here
    if (name == "position") {
      return InternalStructField(NamedNumType(name, type), "position");
    } else {
      size_t location = getNextStructLocation(outputStruct);
      return InternalStructField(NamedNumType(name, type), location);
    }
  }
};

struct DynamicFragmentOutput : public IGeneratorDynamicHandler {
  std::vector<InternalStructField> &outputStruct;

  DynamicFragmentOutput(std::vector<InternalStructField> &outputStruct) : outputStruct(outputStruct) {}

  bool createDynamicOutput(FastString name, NumType requestedType) {
    InternalStructField newField = generateDynamicStructOutput(name, requestedType);
    outputStruct.push_back(newField);
    return true;
  }

  InternalStructField generateDynamicStructOutput(FastString name, const NumType &type) {
    // Handle builtin outputs here
    if (name == "depth") {
      return InternalStructField(NamedNumType(name, type), "frag_depth");
    } else {
      size_t location = getNextStructLocation(outputStruct);
      return InternalStructField(NamedNumType(name, type), location);
    }
  }
};

struct StageIO {
  std::vector<InternalStructField> inputStructFields;
  std::vector<InternalStructField> outputStructFields;
};

struct PipelineIO {
  const std::vector<NamedNumType> &outputFields;
  std::vector<NamedNumType> vertexInputFields;
  StageIO vertexIO;
  StageIO fragmentIO;
  std::vector<NamedNumType> fragmentInputFields;

  std::optional<DynamicVertexInput> dynamicVertexInputHandler;
  std::optional<DynamicVertexOutput> dynamicVertexOutputHandler;
  std::optional<DynamicFragmentOutput> dynamicFragmentOutputHandler;

  PipelineIO(const MeshFormat &meshFormat, const std::vector<NamedNumType> &outputFields) : outputFields(outputFields) {
    for (auto &attr : meshFormat.vertexAttributes) {
      vertexInputFields.emplace_back(attr.name, NumType(getCompatibleShaderFieldBaseType(attr.type), attr.numComponents));
    }

    for (size_t i = 0; i < vertexInputFields.size(); i++) {
      vertexIO.inputStructFields.emplace_back(vertexInputFields[i], i);
    }

    for (size_t i = 0; i < outputFields.size(); i++) {
      fragmentIO.outputStructFields.emplace_back(outputFields[i], i);
    }

    vertexIO.outputStructFields.emplace_back(NamedNumType("instanceIndex", NumType(ShaderFieldBaseType::UInt32, 1)), 0);

    dynamicVertexInputHandler.emplace(vertexIO.inputStructFields);
    dynamicVertexOutputHandler.emplace(vertexIO.outputStructFields);
    dynamicFragmentOutputHandler.emplace(fragmentIO.outputStructFields);
  }

  void setupDefinitions(GeneratorDefinitions &outDefinitions, std::vector<IGeneratorDynamicHandler *> &outDynamics,
                        ProgrammableGraphicsStage stage) {
    static std::vector<NamedNumType> emptyStruct;
    const std::vector<NamedNumType> *inputs{};
    const std::vector<NamedNumType> *outputs{};

    outDynamics.clear();

    switch (stage) {
    case gfx::ProgrammableGraphicsStage::Vertex:
      inputs = &vertexInputFields;
      outputs = &emptyStruct;
      outDynamics.push_back(&dynamicVertexInputHandler.value());
      outDynamics.push_back(&dynamicVertexOutputHandler.value());
      break;
    case gfx::ProgrammableGraphicsStage::Fragment:
      inputs = &fragmentInputFields;
      outputs = &outputFields;
      outDynamics.push_back(&dynamicFragmentOutputHandler.value());
      break;
    }

    outDefinitions.inputs.clear();
    for (auto &input : *inputs) {
      outDefinitions.inputs.insert_or_assign(input.name, input.type);
    }

    outDefinitions.outputs.clear();
    for (auto &outputField : *outputs) {
      outDefinitions.outputs.insert_or_assign(outputField.name, outputField.type);
    }
  }

  static bool isValidFragmentInputBuiltin(FastString builtin) { return builtin == "position"; }

  void interpolateVertexOutputs() {
    for (auto &outputField : vertexIO.outputStructFields) {
      if (outputField.builtinTag.empty() || isValidFragmentInputBuiltin(outputField.builtinTag)) {
        fragmentInputFields.emplace_back(outputField.base);
        fragmentIO.inputStructFields.emplace_back(outputField);
      }
    }
  }
};

static const size_t NumGraphicsStages = 2;
struct Stage {
  ProgrammableGraphicsStage stage;
  std::vector<const EntryPoint *> entryPoints;
  std::vector<String> extraEntryPointParameters;
  String mainFunctionHeader;
  String inputStructName;
  String inputVariableName;
  String outputStructName;
  String outputVariableName;
  String globalsStructName;
  String globalsVariableName;

  Stage(ProgrammableGraphicsStage stage, const String &inputStructName, const String &outputStructName)
      : stage(stage), inputStructName((inputStructName)), outputStructName((outputStructName)) {
    inputVariableName = fmt::format("p_{}_input", magic_enum::enum_name(stage));
    outputVariableName = fmt::format("p_{}_output", magic_enum::enum_name(stage));

    globalsStructName = fmt::format("{}_globals_t", magic_enum::enum_name(stage));
    globalsVariableName = fmt::format("p_{}_globals", magic_enum::enum_name(stage));
  }

  bool sort(bool ignoreMissingDependencies = true) { return sortEntryPoints(entryPoints, ignoreMissingDependencies); }

  void writeInputStructHeader(std::string &output) {
    output += fmt::format("var<private> {}: {};\n", inputVariableName, inputStructName);
  }

  void writeOutputStructHeader(std::string &output) {
    output += fmt::format("var<private> {}: {};\n", outputVariableName, outputStructName);
  }

  StageOutput process(PipelineIO &pipelineIO, StageIO &stageIO, const std::map<FastString, BufferDefinition> &buffers,
                      const std::map<FastString, TextureDefinition> &textureDefinitions) {
    GeneratorContext context;
    context.inputVariableName = inputVariableName;
    context.outputVariableName = outputVariableName;
    context.globalsVariableName = globalsVariableName;

    context.definitions.buffers = buffers;
    context.definitions.textures = textureDefinitions;

    pipelineIO.setupDefinitions(context.definitions, context.dynamicHandlers, stage);

    const char *wgslStageName{};
    switch (stage) {
    case ProgrammableGraphicsStage::Vertex:
      wgslStageName = "vertex";
      break;
    case ProgrammableGraphicsStage::Fragment:
      wgslStageName = "fragment";
      break;
    }

    std::vector<String> functionNamesToCall;
    size_t index = 0;
    for (const EntryPoint *entryPoint : entryPoints) {
      String entryPointFunctionName = fmt::format("entryPoint_{}_{}", wgslStageName, index++);
      functionNamesToCall.push_back(entryPointFunctionName);

      context.write(fmt::format("fn {}() {{\n", entryPointFunctionName));
      entryPoint->code->apply(context);
      context.write("}\n");
    }

    String entryPointParams;
    bool hasInput{};
    if (!stageIO.inputStructFields.empty()) {
      entryPointParams = fmt::format("in: {}", inputStructName);
      hasInput = true;
    }

    if (!extraEntryPointParameters.empty()) {
      if (!entryPointParams.empty())
        entryPointParams += ", ";
      entryPointParams += boost::algorithm::join(extraEntryPointParameters, ", ");
    }

    bool hasOutput{};
    String returnExpr;
    if (!stageIO.outputStructFields.empty()) {
      returnExpr = fmt::format("-> {}", outputStructName);
      hasOutput = true;
    }

    context.write(fmt::format("@{}\nfn {}_main({}) {} {{\n", wgslStageName, wgslStageName, entryPointParams, returnExpr));
    if (hasInput)
      context.write(fmt::format("\t{} = in;\n", inputVariableName));

    context.write("\t" + mainFunctionHeader + "\n");

    for (auto &functionName : functionNamesToCall) {
      context.write(fmt::format("\t{}();\n", functionName));
    }

    if (hasOutput)
      context.write(fmt::format("\treturn {};\n", outputVariableName));
    context.write("}\n");

    String globalsHeader;
    if (!context.definitions.globals.empty()) {
      std::vector<InternalStructField> globalsStructFields;
      for (auto &field : context.definitions.globals) {
        globalsStructFields.emplace_back(NamedNumType(field.first, field.second));
      }
      generateStruct(globalsHeader, globalsStructName, globalsStructFields, false);
      globalsHeader += fmt::format("var<private> {}: {};\n", globalsVariableName, globalsStructName);
    }

    std::string outSource = std::move(globalsHeader);
    for (auto &header : context.headers) {
      outSource += std::move(header);
    }
    outSource += std::move(context.result);

    return StageOutput{
        std::move(outSource),
        std::move(context.errors),
    };
  }
};

GeneratorOutput Generator::build(const std::vector<EntryPoint> &entryPoints) { return build(getEntryPointPtrs(entryPoints)); }

GeneratorOutput Generator::build(const std::vector<const EntryPoint *> &entryPoints) {
  String vertexInputStructName = "Input";
  String vertexOutputStructName = "VertOutput";
  String fragmentInputStructName = "FragInput";
  String fragmentOutputStructName = "Output";

  Stage stages[NumGraphicsStages] = {
      Stage(ProgrammableGraphicsStage::Vertex, vertexInputStructName, vertexOutputStructName),
      Stage(ProgrammableGraphicsStage::Fragment, fragmentInputStructName, fragmentOutputStructName),
  };
  auto &vertexStage = stages[0];

  GeneratorOutput output;
  TypeBuilder typeBuilder;

  for (const auto &entryPoint : entryPoints) {
    stages[size_t(entryPoint->stage)].entryPoints.push_back(entryPoint);
  }

  std::string headerCode;
  headerCode.reserve(2 << 12);

  const char *instanceIndexer = "u_instanceIndex";
  headerCode += fmt::format("var<private> {}: u32;\n", instanceIndexer);

  stages[0].extraEntryPointParameters.push_back("@builtin(instance_index) _instanceIndex: u32");
  stages[0].mainFunctionHeader += fmt::format("{} = _instanceIndex;\n", instanceIndexer);
  stages[1].mainFunctionHeader += fmt::format("{} = {}.instanceIndex;\n", instanceIndexer, stages[1].inputVariableName);

  // Interpolate instance index
  stages[0].mainFunctionHeader += fmt::format("{}.instanceIndex = {};\n", stages[0].outputVariableName, instanceIndexer);

  std::map<FastString, BufferDefinition> buffers;
  for (auto &binding : bufferBindings) {
    String variableName = fmt::format("u_{}", binding.name);

    generateBuffer(headerCode, typeBuilder, variableName, binding.layout, binding.addressSpace, binding.bindGroup,
                   binding.binding, binding.dimension);

    auto &bufferDefinition =
        buffers.insert(std::make_pair(binding.name, BufferDefinition{.variableName = variableName, .layout = binding.layout}))
            .first->second;

    bufferDefinition.dimension = binding.dimension;
  }

  std::map<FastString, TextureDefinition> textureDefinitions;
  for (auto &texture : textureBindingLayout.bindings) {
    TextureDefinition def(fmt::format("t_{}", sanitizeIdentifier(texture.name)),
                          fmt::format("texCoord{}", texture.defaultTexcoordBinding),
                          fmt::format("s_{}", sanitizeIdentifier(texture.name)), texture.type);
    generateTextureVars(headerCode, def, textureBindGroup, texture.binding, texture.defaultSamplerBinding);
    textureDefinitions.insert_or_assign(texture.name, def);
  }

  PipelineIO pipelineIO(meshFormat, outputFields);

  String stagesCode;
  stagesCode.reserve(2 << 12);
  for (size_t i = 0; i < NumGraphicsStages; i++) {
    auto &stage = stages[i];

    bool sorted = stage.sort();
    shassert(sorted);

    StageIO &stageIO = i == 0 ? pipelineIO.vertexIO : pipelineIO.fragmentIO;
    StageOutput stageOutput = stage.process(pipelineIO, stageIO, buffers, textureDefinitions);
    for (auto &error : stageOutput.errors)
      output.errors.emplace_back(error);

    stagesCode += stageOutput.code;

    // Copy interpolated fields from vertex to fragment
    if (&stage == &vertexStage) {
      pipelineIO.interpolateVertexOutputs();
    }
  }

  // Generate input/output structs here since they depend on shader code
  auto &vsIO = pipelineIO.vertexIO;
  if (!vsIO.inputStructFields.empty()) {
    generateStruct(headerCode, vertexInputStructName, vsIO.inputStructFields, false);
    stages[0].writeInputStructHeader(headerCode);
  }
  if (!vsIO.outputStructFields.empty()) {
    generateStruct(headerCode, vertexOutputStructName, vsIO.outputStructFields);
    stages[0].writeOutputStructHeader(headerCode);
  }
  auto &fsIO = pipelineIO.fragmentIO;
  if (!fsIO.inputStructFields.empty()) {
    generateStruct(headerCode, fragmentInputStructName, fsIO.inputStructFields);
    stages[1].writeInputStructHeader(headerCode);
  }
  if (!fsIO.outputStructFields.empty()) {
    generateStruct(headerCode, fragmentOutputStructName, fsIO.outputStructFields, false);
    stages[1].writeOutputStructHeader(headerCode);
  }

  output.wgslSource = typeBuilder.generatedCode + headerCode + stagesCode;

  return output;
}

IndexedBindings Generator::indexBindings(const std::vector<EntryPoint> &entryPoints) {
  return indexBindings(getEntryPointPtrs(entryPoints));
}

template <typename T> static auto &findOrAddIndex(T &arr, FastString name) {
  auto it = std::find_if(arr.begin(), arr.end(), [&](auto &element) { return element.name == name; });
  if (it != arr.end())
    return *it;

  auto &newElement = arr.emplace_back();
  newElement.name = name;
  return newElement;
}

IndexedBindings Generator::indexBindings(const std::vector<const EntryPoint *> &entryPoints) {
  struct IndexerContext : public IGeneratorContext {
    IndexedBindings result;
    GeneratorDefinitions definitions;
    std::vector<IGeneratorDynamicHandler *> dynamicHandlers;

    void write(const StringView &str) {}

    void pushHeaderScope() {}
    void popHeaderScope() {}

    void readGlobal(FastString name) {}
    void beginWriteGlobal(FastString name, const NumType &type) { definitions.globals.insert_or_assign(name, type); }
    void endWriteGlobal() {}

    bool hasInput(FastString name) {
      auto it = definitions.inputs.find(name);
      return it != definitions.inputs.end();
    }

    void readInput(FastString name) {}
    const NumType *getOrCreateDynamicInput(FastString name) {
      NumType newField;
      for (auto &h : dynamicHandlers) {
        if (h->createDynamicInput(name, newField)) {
          return &definitions.inputs.insert_or_assign(name, newField).first->second;
        }
      }

      return nullptr;
    }

    bool hasOutput(FastString name) {
      auto it = definitions.outputs.find(name);
      return it != definitions.outputs.end();
    }

    void writeOutput(FastString name, const NumType &type) {
      auto it = definitions.outputs.find(name);
      if (it == definitions.outputs.end()) {
        definitions.outputs.insert_or_assign(name, type);
        for (auto &h : dynamicHandlers) {
          if (h->createDynamicOutput(name, type))
            break;
        }
      }
    }

    const NumType *getOrCreateDynamicOutput(FastString name, NumType requestedType) {
      for (auto &h : dynamicHandlers) {
        if (h->createDynamicOutput(name, requestedType)) {
          return &definitions.outputs.insert_or_assign(name, requestedType).first->second;
        }
      }

      return nullptr;
    }

    bool hasTexture(FastString name, bool defaultTexcoordRequired = true) { return definitions.textures.contains(name); }
    const TextureDefinition *getTexture(FastString name) { return nullptr; }
    void texture(FastString name) { findOrAddIndex(result.textureBindings, name); }
    void textureDefaultTextureCoordinate(FastString name) { findOrAddIndex(result.textureBindings, name); }
    void textureDefaultSampler(FastString name) { findOrAddIndex(result.textureBindings, name); }

    void readBuffer(FastString fieldName, const NumType &type, FastString bufferName,
                    const Function<void(IGeneratorContext &ctx)> &index) {
      findOrAddIndex(result.bufferBindings, bufferName).accessedFields.insert(std::make_pair(fieldName, type));
      if (index)
        index(*this);
    }
    void refBuffer(FastString bufferName) {
      findOrAddIndex(result.bufferBindings, bufferName);
    }

    void pushError(GeneratorError &&error) {}

    const GeneratorDefinitions &getDefinitions() const { return definitions; }

    const std::string &generateTempVariable() {
      static std::string dummy;
      return dummy;
    }
  } context;

  PipelineIO pipelineIO(meshFormat, outputFields);

  struct Stage {
    std::vector<const EntryPoint *> entryPoints;
  } stages[2];

  for (const auto &entryPoint : entryPoints) {
    stages[size_t(entryPoint->stage)].entryPoints.push_back(entryPoint);
  }

  for (auto &buffer : bufferBindings) {
    context.definitions.buffers.insert_or_assign(buffer.name, BufferDefinition{.layout = buffer.layout});
  }

  for (auto &texture : textureBindingLayout.bindings) {
    context.definitions.textures.emplace(texture.name, "").first->second.type = texture.type;
  }

  for (size_t i = 0; i < NumGraphicsStages; i++) {
    auto stageType = gfx::ProgrammableGraphicsStage(i);
    auto &stage = stages[i];

    bool sorted = sortEntryPoints(stage.entryPoints);
    shassert(sorted);

    pipelineIO.setupDefinitions(context.definitions, context.dynamicHandlers, stageType);

    for (auto &entryPoint : stage.entryPoints) {
      entryPoint->code->apply(context);
    }

    // Copy interpolated fields from vertex to fragment
    if (stageType == ProgrammableGraphicsStage::Vertex) {
      pipelineIO.interpolateVertexOutputs();
    } else {
      for (auto &fragOutput : context.definitions.outputs) {
        context.result.outputs.emplace_back(IndexedOutput{
            .name = fragOutput.first,
            .type = fragOutput.second,
        });
      }
    }
  }

  return context.result;
}

void IndexedBindings::dump() {
  SPDLOG_LOGGER_DEBUG(logger, "Indexed Bindings:");
  for (auto &buffer : bufferBindings) {
    SPDLOG_LOGGER_DEBUG(logger, " buffer[{}]:", buffer.name);
    for (auto &field : buffer.accessedFields)
      SPDLOG_LOGGER_DEBUG(logger, " - {} ({})", field.first, field.second);
  }
  for (auto &texture : textureBindings) {
    SPDLOG_LOGGER_DEBUG(logger, " texture[{}]", texture.name);
  }
  for (auto &output : outputs) {
    SPDLOG_LOGGER_DEBUG(logger, " output[{}]: {}", output.name, output.type);
  }
}

void GeneratorOutput::dumpErrors(const std::shared_ptr<spdlog::logger> &logger, const GeneratorOutput &output) {
  if (!output.errors.empty()) {
    SPDLOG_LOGGER_ERROR(logger, "Failed to generate shader code:");
    SPDLOG_LOGGER_ERROR(logger, "{}\n------------------", output.wgslSource);
    for (auto &error : output.errors) {
      SPDLOG_LOGGER_ERROR(logger, ">  {}", error.what());
    }
  }
}

} // namespace shader
} // namespace gfx

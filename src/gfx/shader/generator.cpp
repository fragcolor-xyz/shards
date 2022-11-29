#include "generator.hpp"
#include "fmt.hpp"
#include "shader/uniforms.hpp"
#include "wgsl_mapping.hpp"
#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <gfx/error_utils.hpp>
#include <gfx/graph.hpp>
#include <magic_enum.hpp>
#include <optional>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include "../log.hpp"

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
  const char *textureFormat = "f32";
  const char *textureType = "texture_2d";

  output += fmt::format("@group({}) @binding({})\n", group, binding);
  output += fmt::format("var {}: {}<{}>;\n", def.variableName, textureType, textureFormat);

  output += fmt::format("@group({}) @binding({})\n", group, samplerBinding);
  output += fmt::format("var {}: sampler;\n", def.defaultSamplerVariableName);
}

// Pads a struct to array stride inside an array body
template <typename T> static void generatePaddingForArrayStruct(T &output, const UniformBufferLayout &layout) {
  size_t alignedSize = layout.getArrayStride();
  assert(alignTo<4>(alignedSize) == alignedSize); // Check multiple of 4
  size_t size4ToPad = (alignedSize - layout.size) / 4;
  if (size4ToPad > 0) {
    output += fmt::format("\t_array_padding_: array<f32, {}>,\n", size4ToPad);
  }
}

// Generate buffer struct and binding definitions in shader source
template <typename T>
static void generateBuffer(T &output, const String &name, BufferType type, size_t group, size_t binding,
                           const UniformBufferLayout &layout, bool isArray = false) {

  String structName = name + "_t";
  output += fmt::format("struct {} {{\n", structName);
  for (size_t i = 0; i < layout.fieldNames.size(); i++) {
    output += fmt::format("\t{}: {},\n", layout.fieldNames[i], getFieldWGSLTypeName(layout.items[i].type));
  }
  generatePaddingForArrayStruct(output, layout);
  output += "};\n";

  // array struct wrapper
  const char *varType = structName.c_str();
  String containerTypeName = name + "_container";
  if (isArray) {
    output += fmt::format("struct {} {{ elements: array<{}> }};\n", containerTypeName, structName);
    varType = containerTypeName.c_str();
  }

  // global storage/uniform variable
  const char *varStorageType = nullptr;
  switch (type) {
  case BufferType::Uniform:
    varStorageType = "uniform";
    break;
  case BufferType::Storage:
    varStorageType = "storage";
    break;
  }
  output += fmt::format("@group({}) @binding({})\n", group, binding);
  output += fmt::format("var<{}> {}: {};\n", varStorageType, name, varType);
}

struct StructField {
  static constexpr size_t NO_LOCATION = ~0;
  NamedField base;
  size_t location = NO_LOCATION;
  String builtinTag;

  StructField() = default;
  StructField(const NamedField &base) : base(base) {}
  StructField(const NamedField &base, size_t location) : base(base), location(location) {}
  StructField(const NamedField &base, const String &builtinTag) : base(base), builtinTag(builtinTag) {}
  bool hasLocation() const { return location != NO_LOCATION; }
  bool hasBuiltinTag() const { return !builtinTag.empty(); }
};

template <typename T>
static void generateStruct(T &output, const String &typeName, const std::vector<StructField> &fields, bool interpolated = true) {
  output += fmt::format("struct {} {{\n", typeName);
  for (auto &field : fields) {
    std::string typeName = getFieldWGSLTypeName(field.base.type);

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
    output += fmt::format("{}: {},\n", field.base.name, typeName);
  }
  output += "};\n";
}

static size_t getNextStructLocation(const std::vector<StructField> &_struct) {
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
  std::unordered_map<std::string, size_t> nodeNames;
  for (size_t i = 0; i < entryPoints.size(); i++) {
    const EntryPoint &entryPoint = *entryPoints[i];
    if (!entryPoint.name.empty())
      nodeNames.insert_or_assign(entryPoint.name, i);
  }

  auto resolveNodeIndex = [&](const std::string &name) -> const size_t * {
    auto it = nodeNames.find(name);
    if (it != nodeNames.end())
      return &it->second;
    return nullptr;
  };

  std::set<std::string> missingDependencies;
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
  for (size_t i = 0; i < sortedIndices.size(); i++) {
    entryPoints.push_back(unsortedEntryPoints[sortedIndices[i]]);
  }
  return true;
}

struct DynamicVertexInput : public IGeneratorDynamicHandler {
  std::vector<StructField> &inputStruct;

  DynamicVertexInput(std::vector<StructField> &inputStruct) : inputStruct(inputStruct) {}

  bool createDynamicInput(const char *name, FieldType &out) {
    if (strcmp(name, "vertex_index") == 0) {
      out = FieldType(ShaderFieldBaseType::UInt32);
      StructField newField = generateDynamicStructInput(name, out);
      inputStruct.push_back(newField);
      return true;
    }
    return false;
  }

  StructField generateDynamicStructInput(const String &name, const FieldType &type) {
    if (name == "vertex_index") {
      return StructField(NamedField(name, type), "vertex_index");
    } else {
      throw std::logic_error("Unknown dynamic vertex input");
    }
  }
};

struct DynamicVertexOutput : public IGeneratorDynamicHandler {
  std::vector<StructField> &outputStruct;

  DynamicVertexOutput(std::vector<StructField> &outputStruct) : outputStruct(outputStruct) {}

  bool createDynamicOutput(const char *name, FieldType requestedType) {
    StructField newField = generateDynamicStructOutput(name, requestedType);
    outputStruct.push_back(newField);
    return true;
  }

  StructField generateDynamicStructOutput(const String &name, const FieldType &type) {
    // Handle builtin outputs here
    if (name == "position") {
      return StructField(NamedField(name, type), "position");
    } else {
      size_t location = getNextStructLocation(outputStruct);
      return StructField(NamedField(name, type), location);
    }
  }
};

struct StageIO {
  const std::vector<NamedField> &outputFields;

  std::vector<NamedField> vertexInputFields;
  std::vector<StructField> vertexInputStructFields;
  std::vector<StructField> fragmentOutputStructFields;
  std::vector<StructField> vertexOutputStructFields;
  std::vector<NamedField> fragmentInputFields;

  std::optional<DynamicVertexInput> dynamicVertexInputHandler;
  std::optional<DynamicVertexOutput> dynamicVertexOutputHandler;

  StageIO(const MeshFormat &meshFormat, const std::vector<NamedField> &outputFields) : outputFields(outputFields) {
    for (auto &attr : meshFormat.vertexAttributes) {
      vertexInputFields.emplace_back(attr.name, FieldType(getCompatibleShaderFieldBaseType(attr.type), attr.numComponents));
    }

    for (size_t i = 0; i < vertexInputFields.size(); i++) {
      vertexInputStructFields.emplace_back(vertexInputFields[i], i);
    }

    for (size_t i = 0; i < outputFields.size(); i++) {
      fragmentOutputStructFields.emplace_back(outputFields[i], i);
    }

    vertexOutputStructFields.emplace_back(NamedField("instanceIndex", FieldType(ShaderFieldBaseType::UInt32, 1)), 0);

    dynamicVertexInputHandler.emplace(vertexInputStructFields);
    dynamicVertexOutputHandler.emplace(vertexOutputStructFields);
  }

  void setupDefinitions(GeneratorDefinitions &outDefinitions, std::vector<IGeneratorDynamicHandler *> &outDynamics,
                        ProgrammableGraphicsStage stage) {
    static std::vector<NamedField> emptyStruct;
    const std::vector<NamedField> *inputs{};
    const std::vector<NamedField> *outputs{};

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

  void interpolateVertexOutputs() {
    for (auto &outputField : vertexOutputStructFields)
      fragmentInputFields.emplace_back(outputField.base);
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

  StageOutput process(StageIO &stageIO, const std::map<String, BufferDefinition> &buffers,
                      const std::map<String, TextureDefinition> &textureDefinitions) {
    GeneratorContext context;
    context.inputVariableName = inputVariableName;
    context.outputVariableName = outputVariableName;
    context.globalsVariableName = globalsVariableName;

    context.definitions.buffers = buffers;
    context.definitions.textures = textureDefinitions;

    stageIO.setupDefinitions(context.definitions, context.dynamicHandlers, stage);

    const char *wgslStageName{};
    switch (stage) {
    case ProgrammableGraphicsStage::Vertex:
      wgslStageName = "vertex";
      break;
    case ProgrammableGraphicsStage::Fragment:
      wgslStageName = "fragment";
      break;
    }

    context.write(fmt::format("var<private> {}: {};\n", inputVariableName, inputStructName));
    context.write(fmt::format("var<private> {}: {};\n", outputVariableName, outputStructName));

    std::vector<String> functionNamesToCall;
    size_t index = 0;
    for (const EntryPoint *entryPoint : entryPoints) {
      String entryPointFunctionName = fmt::format("entryPoint_{}_{}", wgslStageName, index++);
      functionNamesToCall.push_back(entryPointFunctionName);

      context.write(fmt::format("fn {}() {{\n", entryPointFunctionName));
      entryPoint->code->apply(context);
      context.write("}\n");
    }

    String entryPointParams = fmt::format("in: {}", inputStructName);
    if (!extraEntryPointParameters.empty()) {
      entryPointParams += ", " + boost::algorithm::join(extraEntryPointParameters, ", ");
    }

    context.write(
        fmt::format("@{}\nfn {}_main({}) -> {} {{\n", wgslStageName, wgslStageName, entryPointParams, outputStructName));
    context.write(fmt::format("\t{} = in;\n", inputVariableName));

    context.write("\t" + mainFunctionHeader + "\n");

    for (auto &functionName : functionNamesToCall) {
      context.write(fmt::format("\t{}();\n", functionName));
    }

    context.write(fmt::format("\treturn {};\n", outputVariableName));
    context.write("}\n");

    String globalsHeader;
    if (!context.definitions.globals.empty()) {
      std::vector<StructField> globalsStructFields;
      for (auto &field : context.definitions.globals) {
        globalsStructFields.emplace_back(NamedField(field.first, field.second));
      }
      generateStruct(globalsHeader, globalsStructName, globalsStructFields, false);
      globalsHeader += fmt::format("var<private> {}: {};\n", globalsVariableName, globalsStructName);
    }

    return StageOutput{
        globalsHeader + std::move(context.header) + std::move(context.result),
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

  std::map<String, BufferDefinition> buffers;
  for (auto &binding : bufferBindings) {
    String variableName = fmt::format("u_{}", binding.name);

    if (!binding.layout.fieldNames.empty()) {
      bool isArray = binding.indexedPerInstance;
      generateBuffer(headerCode, variableName, binding.type, binding.bindGroup, binding.binding, binding.layout, isArray);
    }

    auto &bufferDefinition =
        buffers.insert(std::make_pair(binding.name, BufferDefinition{.variableName = variableName, .layout = binding.layout}))
            .first->second;
    if (binding.indexedPerInstance) {
      bufferDefinition.indexedBy = instanceIndexer;
    }
  }

  std::map<String, TextureDefinition> textureDefinitions;
  for (auto &texture : textureBindingLayout.bindings) {
    TextureDefinition def;
    def.variableName = "t_" + texture.name;
    def.defaultSamplerVariableName = "s_" + texture.name;
    def.defaultTexcoordVariableName = fmt::format("texCoord{}", texture.defaultTexcoordBinding);
    generateTextureVars(headerCode, def, textureBindGroup, texture.binding, texture.defaultSamplerBinding);
    textureDefinitions.insert_or_assign(texture.name, def);
  }

  StageIO stageIO(meshFormat, outputFields);

  String stagesCode;
  stagesCode.reserve(2 << 12);
  for (size_t i = 0; i < NumGraphicsStages; i++) {
    auto &stage = stages[i];

    bool sorted = stage.sort();
    assert(sorted);

    StageOutput stageOutput = stage.process(stageIO, buffers, textureDefinitions);
    for (auto &error : stageOutput.errors)
      output.errors.emplace_back(error);

    stagesCode += stageOutput.code;

    // Copy interpolated fields from vertex to fragment
    if (&stage == &vertexStage) {
      stageIO.interpolateVertexOutputs();
    }
  }

  // Generate input/output structs here since they depend on shader code
  generateStruct(headerCode, vertexInputStructName, stageIO.vertexInputStructFields, false);
  generateStruct(headerCode, vertexOutputStructName, stageIO.vertexOutputStructFields);
  generateStruct(headerCode, fragmentInputStructName, stageIO.vertexOutputStructFields);
  generateStruct(headerCode, fragmentOutputStructName, stageIO.fragmentOutputStructFields, false);

  output.wgslSource = headerCode + stagesCode;

  return output;
}

IndexedBindings Generator::indexBindings(const std::vector<EntryPoint> &entryPoints) {
  return indexBindings(getEntryPointPtrs(entryPoints));
}

template <typename T> static auto &findOrAddIndex(T &arr, const char *name) {
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
    void writeHeader(const StringView &str) {}

    void readGlobal(const char *name) {}
    void beginWriteGlobal(const char *name, const FieldType &type) { definitions.globals.insert_or_assign(name, type); }
    void endWriteGlobal() {}

    bool hasInput(const char *name) {
      auto it = definitions.inputs.find(name);
      return it != definitions.inputs.end();
    }

    void readInput(const char *name) {}
    const FieldType *getOrCreateDynamicInput(const char *name) {
      FieldType newField;
      for (auto &h : dynamicHandlers) {
        if (h->createDynamicInput(name, newField)) {
          return &definitions.inputs.insert_or_assign(name, newField).first->second;
        }
      }

      return nullptr;
    }

    bool hasOutput(const char *name) {
      auto it = definitions.outputs.find(name);
      return it != definitions.outputs.end();
    }

    void writeOutput(const char *name, const FieldType &type) {
      auto it = definitions.outputs.find(name);
      if (it == definitions.outputs.end()) {
        definitions.outputs.insert_or_assign(name, type);
        for (auto &h : dynamicHandlers) {
          if (h->createDynamicOutput(name, type))
            break;
        }
      }
    }

    bool hasTexture(const char *name, bool defaultTexcoordRequired = true) { return true; }
    const TextureDefinition *getTexture(const char *name) { return nullptr; }
    void texture(const char *name) { findOrAddIndex(result.textureBindings, name); }
    void textureDefaultTextureCoordinate(const char *name) { findOrAddIndex(result.textureBindings, name); }
    void textureDefaultSampler(const char *name) { findOrAddIndex(result.textureBindings, name); }

    void readBuffer(const char *fieldName, const FieldType &type, const char *bufferName) {
      findOrAddIndex(result.bufferBindings, bufferName).accessedFields.insert(std::make_pair(fieldName, type));
    }

    void pushError(GeneratorError &&error) {}

    const GeneratorDefinitions &getDefinitions() const { return definitions; }

    const std::string &generateTempVariable() {
      static std::string dummy;
      return dummy;
    }
  } context;

  StageIO stageIO(meshFormat, outputFields);

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
    context.definitions.textures.insert_or_assign(texture.name, TextureDefinition{});
  }

  for (size_t i = 0; i < NumGraphicsStages; i++) {
    auto stageType = gfx::ProgrammableGraphicsStage(i);
    auto &stage = stages[i];

    bool sorted = sortEntryPoints(stage.entryPoints);
    assert(sorted);

    stageIO.setupDefinitions(context.definitions, context.dynamicHandlers, stageType);

    for (auto &entryPoint : stage.entryPoints) {
      entryPoint->code->apply(context);
    }

    // Copy interpolated fields from vertex to fragment
    if (stageType == ProgrammableGraphicsStage::Vertex) {
      stageIO.interpolateVertexOutputs();
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

void GeneratorOutput::dumpErrors(const GeneratorOutput &output) {
  if (!output.errors.empty()) {
    spdlog::error("Failed to generate shader code:");
    for (auto &error : output.errors) {
      spdlog::error(">  {}", error.error);
    }
  }
}

} // namespace shader
} // namespace gfx

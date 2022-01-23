#include "generator.hpp"
#include "wgsl_mapping.hpp"
#include <algorithm>
#include <gfx/graph.hpp>
#include <magic_enum.hpp>
#include <optional>
#include <spdlog/fmt/fmt.h>

namespace gfx {
namespace shader {

void GeneratorContext::write(const StringView &str) { result += str; }

void GeneratorContext::readGlobal(const char *name) {
  auto it = writtenGlobals.find(name);
  assert(it != writtenGlobals.end());
  result += fmt::format("{}.{}", globalsVariableName, name);
}

void GeneratorContext::writeGlobal(const char *name, const FieldType &type) {
  auto it = writtenGlobals.find(name);
  if (it != writtenGlobals.end())
    assert(it->second == type);
  writtenGlobals.insert_or_assign(name, type);
  result += fmt::format("{}.{}", globalsVariableName, name);
}

bool GeneratorContext::hasInput(const char *name) { return inputs.find(name) != inputs.end(); }

void GeneratorContext::readInput(const char *name) {
  auto it = inputs.find(name);
  assert(it != inputs.end());

  result += fmt::format("{}.{}", inputVariableName, name);
}

void GeneratorContext::writeOutput(const char *name) { result += fmt::format("{}.{}", outputVariableName, name); }

void GeneratorContext::readBuffer(const char *name) {
  auto bufferIt = buffers.find(name);
  assert(bufferIt != buffers.end());

  const BufferDefinition &buffer = bufferIt->second;
  if (buffer.indexedBy) {
    result += fmt::format("{}.elements[{}]", buffer.bufferVariableName, *buffer.indexedBy);
  } else {
    result += buffer.bufferVariableName;
  }
}

enum class BufferType {
  Uniform,
  Storage,
};

template <typename T>
static void generateBuffer(T &output, const String &name, BufferType type, size_t group, size_t binding,
                           const UniformBufferLayout &layout, bool isArray = false) {

  String structName = name + "_t";
  output += fmt::format("struct {} {{\n", structName);
  for (size_t i = 0; i < layout.fieldNames.size(); i++) {
    output += fmt::format("\t{}: {};\n", layout.fieldNames[i], getParamWGSLTypeName(layout.items[i].type));
  }
  output += "};\n";

  // array struct wrapper
  const char *varType = structName.c_str();
  String containerTypeName = name + "_container";
  if (isArray) {
    output += fmt::format("struct {} {{ elements: array<{}>; }};\n", containerTypeName, structName);
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
  output += fmt::format("[[group({}), binding({})]]\n", group, binding);
  output += fmt::format("var<{}> {}: {};\n", varStorageType, name, varType);
}

struct StructField {
  NamedField base;
  bool autoAssignLocation = false;
  String builtinTag;

  StructField() = default;
  StructField(const NamedField &base, bool autoAssignLocation = false, const String &builtinTag = String())
      : base(base), autoAssignLocation(autoAssignLocation), builtinTag(builtinTag) {}
};

template <typename T> static void generateStruct(T &output, const String &typeName, const std::vector<StructField> &fields) {
  output += fmt::format("struct {} {{\n", typeName);
  size_t index = 0;
  for (auto &field : fields) {
    std::string typeName = getFieldWGSLTypeName(field.base.type);
    if (!field.builtinTag.empty()) {
      output += fmt::format("\t[[builtin({})]] ", field.builtinTag);
      output += fmt::format("{}: {};\n", field.base.name, typeName);
    } else {
      if (field.autoAssignLocation) {
        output += fmt::format("\t[[location({})]] ", index++);
      }
      output += fmt::format("{}: {};\n", field.base.name, typeName);
    }
  }
  output += "};\n";
}

struct Stage {
  ProgrammableGraphicsStage stage;
  std::vector<const EntryPoint *> entryPoints;
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

  bool sort(bool ignoreMissingDependencies = true) {
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

  String process(const std::vector<NamedField> &inputs, const std::vector<NamedField> &outputs,
                 const std::map<String, BufferDefinition> &buffers) {
    GeneratorContext context;
    context.buffers = buffers;
    context.inputVariableName = inputVariableName;
    context.outputVariableName = outputVariableName;
    context.globalsVariableName = globalsVariableName;

    for (auto &input : inputs) {
      context.inputs.insert_or_assign(input.name, &input);
    }

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

    context.write(fmt::format("[[stage({})]]\nfn {}_main(in: {}) -> {} {{\n", wgslStageName, wgslStageName, inputStructName,
                              outputStructName));
    context.write(fmt::format("\t{} = in;\n", inputVariableName));

    context.write("\t" + mainFunctionHeader + "\n");

    for (auto &functionName : functionNamesToCall) {
      context.write(fmt::format("\t{}();\n", functionName));
    }

    context.write(fmt::format("\treturn {};\n", outputVariableName));
    context.write("}\n");

    String globalsHeader;
    if (!context.writtenGlobals.empty()) {
      std::vector<StructField> globalsStructFields;
      for (auto &field : context.writtenGlobals) {
        globalsStructFields.emplace_back(NamedField(field.first, field.second));
      }
      generateStruct(globalsHeader, globalsStructName, globalsStructFields);
      globalsHeader += fmt::format("var<private> {}: {};\n", globalsVariableName, globalsStructName);
    }

    return globalsHeader + std::move(context.result);
  }
};

GeneratorOutput Generator::build(const std::vector<EntryPoint> &_entryPoints) {
  std::vector<const EntryPoint *> entryPoints;
  std::transform(_entryPoints.begin(), _entryPoints.end(), std::back_inserter(entryPoints),
                 [](const EntryPoint &entryPoint) { return &entryPoint; });
  return build(entryPoints);
}

GeneratorOutput Generator::build(const std::vector<const EntryPoint *> &entryPoints) {
  String vertexInputStructName = "Input";
  String vertexOutputStructName = "VertOutput";
  String fragmentInputStructName = "FragInput";
  String fragmentOutputStructName = "Output";

  const size_t numStages = 2;
  Stage stages[2] = {
      Stage(ProgrammableGraphicsStage::Vertex, vertexInputStructName, vertexOutputStructName),
      Stage(ProgrammableGraphicsStage::Fragment, fragmentInputStructName, fragmentOutputStructName),
  };

  GeneratorOutput output;

  for (const auto &entryPoint : entryPoints) {
    stages[size_t(entryPoint->stage)].entryPoints.push_back(entryPoint);
  }

  std::string result;
  result.reserve(2 << 12);

  const char *instanceIndexer = "u_instanceIndex";
  result += fmt::format("var<private> {}: u32;\n", instanceIndexer);

  std::vector<NamedField> vertexInputFields;
  for (auto &attr : meshFormat.vertexAttributes) {
    vertexInputFields.emplace_back(attr.name, FieldType(getCompatibleShaderFieldBaseType(attr.type), attr.numComponents));
  }

  std::vector<StructField> vertexInputStructFields;
  std::transform(vertexInputFields.begin(), vertexInputFields.end(), std::back_inserter(vertexInputStructFields),
                 [](const NamedField &field) { return StructField(field, true); });

  std::vector<StructField> fragmentOutputStructFields;
  std::transform(outputFields.begin(), outputFields.end(), std::back_inserter(fragmentOutputStructFields),
                 [](const NamedField &field) { return StructField(field, true); });

  std::vector<StructField> vertexOutputStructFields;
  std::transform(interpolatedFields.begin(), interpolatedFields.end(), std::back_inserter(vertexOutputStructFields),
                 [](const NamedField &field) {
                   String tag;
                   if (field.name == "position")
                     tag = "position";
                   return tag.empty() ? StructField(field, true) : StructField(field, false, tag);
                 });

  std::vector<StructField> fragmentInputStructFields = vertexOutputStructFields;

  // Add instance index
  vertexInputStructFields.emplace_back(NamedField("instanceIndex", FieldType(ShaderFieldBaseType::UInt32, 1)), false,
                                       "instance_index");
  vertexOutputStructFields.emplace_back(NamedField("instanceIndex", FieldType(ShaderFieldBaseType::UInt32, 1)), true);
  fragmentInputStructFields.emplace_back(NamedField("instanceIndex", FieldType(ShaderFieldBaseType::UInt32, 1)), true);
  for (auto &stage : stages) {
    stage.mainFunctionHeader += fmt::format("{} = {}.instanceIndex;\n", instanceIndexer, stage.inputVariableName);
  }

  // Interpolate instance index
  stages[0].mainFunctionHeader += fmt::format("{}.instanceIndex = {};\n", stages[0].outputVariableName, instanceIndexer);

  generateStruct(result, vertexInputStructName, vertexInputStructFields);
  generateStruct(result, vertexOutputStructName, vertexOutputStructFields);
  generateStruct(result, fragmentInputStructName, fragmentInputStructFields);
  generateStruct(result, fragmentOutputStructName, fragmentOutputStructFields);

  generateBuffer(result, "u_view", BufferType::Uniform, 0, 0, viewBufferLayout);
  generateBuffer(result, "u_objects", BufferType::Storage, 0, 1, objectBufferLayout, true);

  std::map<String, BufferDefinition> buffers = {
      {"view", {"u_view"}},
      {"object", {"u_objects", instanceIndexer}},
  };

  for (size_t i = 0; i < numStages; i++) {
    auto &stage = stages[i];

    bool sorted = stage.sort();
    assert(sorted);

    std::vector<NamedField> *inputs{};
    std::vector<NamedField> *outputs{};
    switch (stage.stage) {
    case gfx::ProgrammableGraphicsStage::Vertex:
      inputs = &vertexInputFields;
      outputs = &interpolatedFields;
      break;
    case gfx::ProgrammableGraphicsStage::Fragment:
      inputs = &interpolatedFields;
      outputs = &outputFields;
      break;
    }

    result += stage.process(*inputs, *outputs, buffers);
  }

  output.wgslSource = result;

  return output;
}
} // namespace shader
} // namespace gfx

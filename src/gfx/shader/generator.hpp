#pragma once

#include "block.hpp"
#include "entry_point.hpp"
#include "types.hpp"
#include "uniforms.hpp"
#include <gfx/mesh.hpp>
#include <gfx/params.hpp>
#include <map>
#include <optional>
#include <string>

namespace gfx {
namespace shader {
using String = std::string;
using StringView = std::string_view;

struct GeneratorContext {
  String result;
  String inputVariableName;
  String outputVariableName;
  String globalsVariableName;
  std::map<String, BufferDefinition> buffers;
  std::map<String, const NamedField *> inputs;
  std::map<String, FieldType> writtenGlobals;

  void write(const StringView &str);

  void readGlobal(const char *name);
  void writeGlobal(const char *name, const FieldType &type);

  bool hasInput(const char *name);
  void readInput(const char *name);
  void writeOutput(const char *name);

  void readBuffer(const char *name);
};

struct GeneratorOutput {
  String wgslSource;
};

struct Generator {
  UniformBufferLayout objectBufferLayout;
  UniformBufferLayout viewBufferLayout;
  MeshFormat meshFormat;
  std::vector<NamedField> interpolatedFields;
  std::vector<NamedField> outputFields;

  GeneratorOutput build(const std::vector<EntryPoint> &entryPoints);
  GeneratorOutput build(const std::vector<const EntryPoint *> &entryPoints);
};
} // namespace shader
} // namespace gfx

#ifndef GFX_SHADER_GENERATOR
#define GFX_SHADER_GENERATOR

#include "block.hpp"
#include "entry_point.hpp"
#include "textures.hpp"
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

struct GeneratorError {
  std::string error;
};

enum class BufferType {
  Uniform,
  Storage,
};

struct IGeneratorDynamicHandler {
  // Entry point for generating stage inputs on-demand
  // return true to indicate that the input now exists and the out field is filled in
  virtual bool createDynamicInput(const char *name, FieldType &out) { return false; }
  // Entry point for generating stage outputs on-demand
  // return true to indicate that the output now exists
  virtual bool createDynamicOutput(const char *name, FieldType requestedType) { return false; }
};

template <typename... TArgs> static GeneratorError formatError(const char *format, TArgs... args) {
  return GeneratorError{fmt::format(format, args...)};
}

struct IGeneratorContext {
  // Write directly to output stream
  virtual void write(const StringView &str) = 0;
  // Writes into a separate buffer that is prepended to the combined output of write and all other generated code
  virtual void writeHeader(const StringView &str) = 0;

  virtual void readGlobal(const char *name) = 0;
  virtual void beginWriteGlobal(const char *name, const FieldType &type) = 0;
  virtual void endWriteGlobal() = 0;

  template <typename T> void writeGlobal(const char *name, const FieldType &type, T &&inner) {
    beginWriteGlobal(name, type);
    inner();
    endWriteGlobal();
  }

  virtual bool hasInput(const char *name) = 0;
  virtual void readInput(const char *name) = 0;
  virtual const std::map<String, FieldType> &getInputs() = 0;

  virtual bool hasOutput(const char *name) = 0;
  virtual void writeOutput(const char *name, const FieldType &type) = 0;

  virtual bool hasTexture(const char *name, bool defaultTexcoordRequired = true) = 0;
  virtual const TextureDefinition *getTexture(const char *name) = 0;
  virtual void texture(const char *name) = 0;
  virtual void textureDefaultTextureCoordinate(const char *name) = 0;
  virtual void textureDefaultSampler(const char *name) = 0;

  virtual void readBuffer(const char *fieldName, const FieldType &type, const char *bufferName) = 0;
  virtual const UniformLayout *findUniform(const char *fieldName, const BufferDefinition &buffer) = 0;

  virtual void pushError(GeneratorError &&error) = 0;
};

struct GeneratorContext : public IGeneratorContext {
  String result;
  String header;
  String inputVariableName;
  String outputVariableName;
  String globalsVariableName;
  std::map<String, BufferDefinition> buffers;
  std::map<String, TextureDefinition> textures;
  std::map<String, FieldType> inputs;
  std::map<String, FieldType> globals;
  std::map<String, FieldType> outputs;
  std::map<String, size_t> sampleTextures;
  bool canAddOutputs = false;
  std::vector<GeneratorError> errors;

  std::vector<IGeneratorDynamicHandler *> dynamicHandlers;

  void write(const StringView &str);

  // Writes into a separate buffer that is prepended to the combined output of write and all other generated code
  void writeHeader(const StringView &str);

  void readGlobal(const char *name);
  void beginWriteGlobal(const char *name, const FieldType &type);
  void endWriteGlobal();

  bool hasInput(const char *name);
  void readInput(const char *name);
  const std::map<String, FieldType> &getInputs() { return inputs; }
  const FieldType *getOrCreateDynamicInput(const char *name);

  bool hasOutput(const char *name);
  void writeOutput(const char *name, const FieldType &type);
  const FieldType *getOrCreateDynamicOutput(const char *name, FieldType requestedType);

  bool hasTexture(const char *name, bool defaultTexcoordRequired = true);
  const TextureDefinition *getTexture(const char *name);
  void texture(const char *name);
  void textureDefaultTextureCoordinate(const char *name);
  void textureDefaultSampler(const char *name);

  void readBuffer(const char *fieldName, const FieldType &type, const char *bufferName);
  const UniformLayout *findUniform(const char *fieldName, const BufferDefinition &buffer);

  void pushError(GeneratorError &&error);
};

struct GeneratorOutput {
  String wgslSource;
  std::vector<GeneratorError> errors;

  static void dumpErrors(const GeneratorOutput &output);
};

struct BufferBinding {
  size_t bindGroup{};
  size_t binding{};
  std::string name;
  UniformBufferLayout layout;
  BufferType type;
  bool indexedPerInstance{};
};

struct IndexedBufferBinding {
  std::string name;
  std::map<std::string, FieldType> accessedFields;
};

struct IndexedTextureBinding {
  std::string name;
};

struct IndexedOutput {
  std::string name;
  FieldType type;
};

struct IndexedBindings {
  std::vector<IndexedBufferBinding> bufferBindings;
  std::vector<IndexedTextureBinding> textureBindings;
  std::vector<IndexedOutput> outputs;

  void dump();
};

struct Generator {
  std::vector<BufferBinding> bufferBindings;
  TextureBindingLayout textureBindingLayout;
  size_t textureBindGroup{};
  MeshFormat meshFormat;
  std::vector<NamedField> outputFields;

  IndexedBindings indexBindings(const std::vector<EntryPoint> &entryPoints);
  IndexedBindings indexBindings(const std::vector<const EntryPoint *> &entryPoints);
  GeneratorOutput build(const std::vector<EntryPoint> &entryPoints);
  GeneratorOutput build(const std::vector<const EntryPoint *> &entryPoints);
};
} // namespace shader
} // namespace gfx

#endif // GFX_SHADER_GENERATOR

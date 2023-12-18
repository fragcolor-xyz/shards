#ifndef GFX_SHADER_GENERATOR
#define GFX_SHADER_GENERATOR

#include "../../core/function.hpp"
#include "block.hpp"
#include "entry_point.hpp"
#include "spdlog/logger.h"
#include "textures.hpp"
#include "types.hpp"
#include "uniforms.hpp"
#include "temp_variable.hpp"
#include <gfx/mesh.hpp>
#include <gfx/params.hpp>
#include <map>
#include <memory>
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

template <typename Sig> using Function = shards::FunctionBase<128, Sig>;

struct IGeneratorDynamicHandler {
  // Entry point for generating stage inputs on-demand
  // return true to indicate that the input now exists and the out field is filled in
  virtual bool createDynamicInput(FastString name, NumFieldType &out) { return false; }
  // Entry point for generating stage outputs on-demand
  // return true to indicate that the output now exists
  virtual bool createDynamicOutput(FastString name, NumFieldType requestedType) { return false; }
};

template <typename... TArgs> static GeneratorError formatError(const char *format, TArgs... args) {
  return GeneratorError{fmt::format(format, args...)};
}

struct GeneratorDefinitions {
  std::map<FastString, BufferDefinition> buffers;
  std::map<FastString, TextureDefinition> textures;
  std::map<FastString, NumFieldType> inputs;
  std::map<FastString, NumFieldType> globals;
  std::map<FastString, NumFieldType> outputs;
};

struct IGeneratorContext {
  // Write directly to output stream
  virtual void write(const StringView &str) = 0;

  // Push / pop for defining code outside of the current function
  // any context writes in between these calls will be written to a header location before the current function being written
  virtual void pushHeaderScope() = 0;
  virtual void popHeaderScope() = 0;

  virtual void readGlobal(FastString name) = 0;
  virtual void beginWriteGlobal(FastString name, const NumFieldType &type) = 0;
  virtual void endWriteGlobal() = 0;

  virtual bool hasInput(FastString name) = 0;
  virtual void readInput(FastString name) = 0;
  virtual const NumFieldType *getOrCreateDynamicInput(FastString name) = 0;

  virtual bool hasOutput(FastString name) = 0;
  virtual void writeOutput(FastString name, const NumFieldType &type) = 0;
  virtual const NumFieldType *getOrCreateDynamicOutput(FastString name, NumFieldType requestedType) = 0;

  virtual bool hasTexture(FastString name, bool defaultTexcoordRequired = true) = 0;
  virtual const TextureDefinition *getTexture(FastString name) = 0;
  virtual void texture(FastString name) = 0;
  virtual void textureDefaultTextureCoordinate(FastString name) = 0;
  virtual void textureDefaultSampler(FastString name) = 0;

  virtual void readBuffer(FastString fieldName, const NumFieldType &type, FastString bufferName,
                          const Function<void(IGeneratorContext &ctx)> &index = Function<void(IGeneratorContext &ctx)>()) = 0;

  virtual const GeneratorDefinitions &getDefinitions() const = 0;

  virtual void pushError(GeneratorError &&error) = 0;

  virtual const std::string &generateTempVariable() = 0;

  // Helper function for writeGlobal
  template <typename T> void writeGlobal(FastString name, const NumFieldType &type, T &&inner) {
    beginWriteGlobal(name, type);
    inner();
    endWriteGlobal();
  }
};

struct GeneratorContext : public IGeneratorContext {
  String result;

  std::vector<std::string> headers;
  std::vector<size_t> headerStack;

  String inputVariableName;
  String outputVariableName;
  String globalsVariableName;
  GeneratorDefinitions definitions;
  bool canAddOutputs = false;
  std::vector<GeneratorError> errors;

  std::vector<IGeneratorDynamicHandler *> dynamicHandlers;

  TempVariableAllocator tempVariableAllocator;

  std::string &getOutput();

  void write(const StringView &str);

  void pushHeaderScope();
  void popHeaderScope();

  // Writes into a separate buffer that is prepended to the combined output of write and all other generated code
  void writeHeader(const StringView &str);

  void readGlobal(FastString name);
  void beginWriteGlobal(FastString name, const NumFieldType &type);
  void endWriteGlobal();

  bool hasInput(FastString name);
  void readInput(FastString name);
  const NumFieldType *getOrCreateDynamicInput(FastString name);

  bool hasOutput(FastString name);
  void writeOutput(FastString name, const NumFieldType &type);
  const NumFieldType *getOrCreateDynamicOutput(FastString name, NumFieldType requestedType);

  bool hasTexture(FastString name, bool defaultTexcoordRequired = true);
  const TextureDefinition *getTexture(FastString name);
  void texture(FastString name);
  void textureDefaultTextureCoordinate(FastString name);
  void textureDefaultSampler(FastString name);

  void readBuffer(FastString fieldName, const NumFieldType &type, FastString bufferName,
                  const Function<void(IGeneratorContext &ctx)> &index);

  void pushError(GeneratorError &&error);

  const GeneratorDefinitions &getDefinitions() const { return definitions; }

  const std::string &generateTempVariable() { return tempVariableAllocator.get(); }
};

struct GeneratorOutput {
  String wgslSource;
  std::vector<GeneratorError> errors;

  static void dumpErrors(const std::shared_ptr<spdlog::logger> &logger, const GeneratorOutput &output);
};

struct BufferBinding {
  size_t bindGroup{};
  size_t binding{};
  FastString name;
  UniformBufferLayout layout;
  BufferType type = BufferType::Uniform;
  Dimension dimension;
};

struct IndexedBufferBinding {
  FastString name;
  std::map<FastString, NumFieldType> accessedFields;
};

struct IndexedTextureBinding {
  FastString name;
};

struct IndexedOutput {
  FastString name;
  NumFieldType type;
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

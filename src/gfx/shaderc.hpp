#pragma once
#include "bgfx/bgfx.h"
#include "enums.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gfx {
struct Context;

struct ShaderCompileContext {
  std::vector<std::string> errors;
  std::vector<uint8_t> binary;
};

struct ShaderCompilePODOptions {
  char shaderType = ' ';
  bool disasm = false;
  bool raw = false;
  bool preprocessOnly = false;
  bool depends = false;
  bool debugInformation = false;
  bool avoidFlowControl = false;
  bool noPreshader = false;
  bool partialPrecision = false;
  bool preferFlowControl = false;
  bool backwardsCompatibility = false;
  bool warningsAreErrors = false;
  bool keepIntermediate = false;
  bool optimize = false;
  bool verbose = false;
  bool keepOutputs = true;
  uint32_t optimizationLevel = 3;
};

struct IShaderCompileOptions {
  virtual const ShaderCompilePODOptions &getPODOptions() = 0;
  virtual const char *getInputFilePath() = 0;
  virtual const char *getOutputFilePath() = 0;
  virtual const char *getPlatform() = 0;
  virtual const char *getProfile() = 0;
  virtual const char *getIncludeDir(size_t index) = 0;
  virtual const char *getDefine(size_t index) = 0;
  virtual const char *getDependency(size_t index) = 0;
  virtual size_t getNumIncludeDirs() = 0;
  virtual size_t getNumDefines() = 0;
  virtual size_t getNumDependencies() = 0;
};

struct IShaderCompileOutput {
  virtual void writeError(const char *error) = 0;
  virtual void writeOutput(const void *data, int32_t size) = 0;
};

struct ShaderCompileOutput : public IShaderCompileOutput {
  std::vector<uint8_t> binary;
  std::vector<std::string> errors;

  virtual void writeError(const char *error);
  virtual void writeOutput(const void *data, int32_t size);
};

struct ShaderCompileOptions : public ShaderCompilePODOptions,
                              public IShaderCompileOptions {
  std::string platform;
  std::string profile;

  std::string inputFilePath;
  std::string outputFilePath;

  std::vector<std::string> includeDirs;
  std::vector<std::string> defines;
  std::vector<std::string> dependencies;

  ShaderCompileOptions();

  void setBuiltinIncludePaths();
  void setCompatibleForContext(Context &context);
  void setCompatibleForRendererType(RendererType type);

  const ShaderCompilePODOptions &getPODOptions() { return *this; }
  const char *getInputFilePath() { return inputFilePath.c_str(); }
  const char *getOutputFilePath() { return outputFilePath.c_str(); }
  const char *getPlatform() { return platform.c_str(); }
  const char *getProfile() { return profile.c_str(); }
  const char *getIncludeDir(size_t index) { return includeDirs[index].c_str(); }
  const char *getDefine(size_t index) { return defines[index].c_str(); }
  const char *getDependency(size_t index) {
    return dependencies[index].c_str();
  }
  size_t getNumIncludeDirs() { return includeDirs.size(); }
  size_t getNumDefines() { return defines.size(); }
  size_t getNumDependencies() { return dependencies.size(); }
};

struct IShaderCompiler {
  virtual ~IShaderCompiler() = default;
  virtual bool compile(IShaderCompileOptions &options,
                       IShaderCompileOutput &output, const char *varying,
                       const char *code, size_t codeLength) = 0;
};

class ShaderCompilerModule {
private:
  IShaderCompiler *instance = nullptr;
  void *moduleHandle = nullptr;

public:
  ShaderCompilerModule(IShaderCompiler *instance, void *moduleHandle = nullptr);
  ~ShaderCompilerModule();
  IShaderCompiler &getShaderCompiler();
};
std::shared_ptr<ShaderCompilerModule> createShaderCompilerModule();
} // namespace gfx
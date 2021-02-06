/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

// collection of runtime chains to allow shader compilation, caching and
// storage.

#include <utility.hpp>
// include this last
#include <chain_dsl.hpp>

#ifdef _WIN32
#define Platform_Name() let("windows")
#elif defined(__APPLE__)
#define Platform_Name() let("ios")
#elif defined(__linux__)
#define Platform_Name() let("linux")
#endif

#ifdef _WIN32
#define Shader_Model() let("s_5")
#elif defined(__APPLE__)
#define Shader_Model() let("metal")
#elif defined(__linux__)
#define Shader_Model() let("430")
#endif

#ifdef _WIN32
#define Shaderc_Command(_args) Process_Run("shadercRelease.exe", _args);
#else
#define Shaderc_Command(_args) Wasm_Run("shadercRelease.exe", _args);
#endif

#define Compile_Shader(_type)                                                  \
  let("-f")                                                                    \
      .Push(args)                                                              \
      .let(_type "-shader-tmp.txt")                                            \
      .Push(args)                                                              \
      .let("-o")                                                               \
      .Push(args)                                                              \
      .let(_type "-shader-tmp.bin")                                            \
      .Push(args)                                                              \
      .let("--platform")                                                       \
      .Push(args)                                                              \
      .Platform_Name()                                                         \
      .Push(args)                                                              \
      .let("-p")                                                               \
      .Push(args)                                                              \
      .Shader_Model()                                                          \
      .Push(args)                                                              \
      .let("--type")                                                           \
      .Push(args)                                                              \
      .let(_type)                                                              \
      .Push(args)                                                              \
      .Shaderc_Command(args)

namespace chainblocks {
struct ShaderCompiler {
  ShaderCompiler() {
    DefChain(shader_compiler)
        .Input()
        .Take(0)
        .Set(varyings)
        .Input()
        .Take(1)
        .Set(vs)
        .Input()
        .Take(2)
        .Set(fs)
        .Input()
        .Take(3)
        .Set(defines)
        .let("varying.def.sc")
        .FS_Write_Overwriting(varyings)
        .let("v-shader-tmp.txt")
        .FS_Write_Overwriting(vs)
        .let("f-shader-tmp.txt")
        .FS_Write_Overwriting(fs)
        .Compile_Shader("v");
    _chain = shader_compiler;
  }

private:
  chainblocks::Chain _chain;
};

Shared<ShaderCompiler> shaderc;

void compileShader() {}
} // namespace chainblocks
/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

// collection of runtime chains to allow shader compilation, caching and
// storage.

#include "./bgfx.hpp"
#include <runtime.hpp>
#include <utility.hpp>
// include this last
#include <chain_dsl.hpp>

#ifdef _WIN32
#define Platform_Name() let("windows")
#elif defined(__APPLE__)
#define Platform_Name() let("ios")
#elif defined(__linux__)
#define Platform_Name() let("linux")
#elif defined(__EMSCRIPTEN__)
#define Platform_Name() let("asm.js")
#endif

#ifdef _WIN32
#define Shader_Model()                                                         \
  Get(shader_type)                                                             \
      .If(Is("v"), let("vs_5_0"), If(Is("f"), let("ps_5_0"), let("cs_5_0")))
#elif defined(__APPLE__)
#define Shader_Model() let("metal")
#elif defined(__linux__)
#if BGFX_OPENGL_VERSION == 33
// headless tests run at version 3.3 due to xvfb limitations
#define Shader_Model() let("330")
#else
#define Shader_Model() let("430")
#endif
#elif defined(__EMSCRIPTEN__)
#define Shader_Model() let("300_es")
#endif

#ifdef _WIN32
#define Shaderc_Command(_args)                                                 \
  let("").Process_Run("shaders/shadercRelease.exe", _args)
#else
#define Shaderc_Command(_args)                                                 \
  let("").Wasm_Run("shaders/shadercRelease.wasm", _args)
#endif

#ifndef NDEBUG
#ifndef _WIN32
// windows shaders are binary (also spirv but we don't use atm)
#define Debug_Print() Ref(bin_path).FS_Read().Log().Get(bin_path)
#else
#define Debug_Print()
#endif
#else
#define Debug_Print()
#endif

chainblocks::Var empty_bytes((uint8_t *)nullptr, 0);

#define Compile_Shader(_type)

namespace chainblocks {
struct ShaderCompiler : public IShaderCompiler {
  ShaderCompiler() {
    /*
     * Inputs:
     * 0: varyings
     * 1: shader code
     * 2: shader type (v, f, c)
     * 3: defines (semicolon separated)
     * Outputs:
     * Bytes of the compiled shader
     */
    DefChain(shader_compiler)
        // unpack input arguments
        .Input()
        .Take(0) Ref(varyings)
        // also fill the hash data sequence
        Push(shader_hashing)
        .Input()
        .Take(1) Ref(shader_code)
        // also fill the hash data sequence
        Push(shader_hashing)
        .Input()
        .Take(2) Ref(shader_type)
        // also fill the hash data sequence
        Push(shader_hashing)
        .Input()
        .Take(3) Ref(defines)
        // also fill the hash data sequence
        Push(shader_hashing)
        // also add platform specifics
        .Platform_Name() Push(shader_hashing)
        .Shader_Model() Push(shader_hashing)
        // hash the shader code and other parameters
        .Get(shader_hashing)
        .Hash()
        .ToHex() Set(shader_hash_filename)
        .let("shaders/cache/") PrependTo(shader_hash_filename) //
        .Get(shader_hash_filename)
        .Maybe( // try to load from cache
            FS_Read_Bytes().Brotli_Decompress(),
            // if cache fails compile and cache
            // write temporary files
            let("shaders/tmp/varying.txt")
                .FS_Write_Overwriting(varyings)
                .let("shaders/tmp/shader.txt")
                .FS_Write_Overwriting(shader_code)
                // populate command arguments
                .let("-f") Push(args)
                .let("shaders/tmp/shader.txt") Push(args)
                .let("-o") Push(args)
                .let("shaders/tmp/shader.bin") Push(args)
                .let("--varyingdef") Push(args)
                .let("shaders/tmp/varying.txt") Push(args)
                .let("--platform") Push(args)
                .Platform_Name() Push(args)
                .let("-p") Push(args)
                .Shader_Model() Push(args)
                .let("--type") Push(args)
                .Get(shader_type) Push(args)
                .let("-i") Push(args)
                .let("shaders/include") Push(args)
                .Count(defines)
                .When(IsNot(0),
                      let("--define") Push(args) //
                          .Get(defines) Push(args))
                .Shaderc_Command(args)
                // read the temporary binary file result
                .let("shaders/tmp/shader.bin") Debug_Print()
                .FS_Read_Bytes() Ref(shader_bytecode)
                .Brotli_Compress() Ref(shader_bytecode_compressed)
                .Get(shader_hash_filename)
                .FS_Write_Overwriting(shader_bytecode_compressed)
                .Get(shader_bytecode));

    _chain = shader_compiler;
  }

  virtual ~ShaderCompiler() {}

  CBVar compile(std::string_view varyings, //
                std::string_view code,     //
                std::string_view type,     //
                std::string_view defines   //
                ) override {
    Var v_varyings{varyings.data(), varyings.size()};
    Var v_code{code.data(), code.size()};
    Var v_type{type.data(), type.size()};
    Var v_defines{defines.data(), defines.size()};
    std::vector<Var> args = {v_varyings, v_code, v_type, v_defines};
    Var v_args{args};
    auto node = CBNode::make();
    node->schedule(_chain, v_args);
    while (!node->empty()) {
      if (!node->tick())
        break;
    }
    auto errors = node->errors();
    if (errors.size() > 0) {
      for (auto &error : errors) {
        LOG(ERROR) << error;
      }
      throw CBException(
          "Shader compiler failed to compile, check errors above.");
    }
    if (_chain->finishedOutput.valueType != CBType::Bytes) {
      throw CBException("Shader compiler failed to compile, no output.");
    }
    return _chain->finishedOutput;
  }

private:
  chainblocks::Chain _chain;
};

std::unique_ptr<IShaderCompiler> makeShaderCompiler() {
  return std::make_unique<ShaderCompiler>();
}
} // namespace chainblocks
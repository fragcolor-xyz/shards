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
#define Shader_Model() let("s_5")
#elif defined(__APPLE__)
#define Shader_Model() let("metal")
#elif defined(__linux__)
#define Shader_Model() let("430")
#elif defined(__EMSCRIPTEN__)
#define Shader_Model() let("300_es")
#endif

#ifdef _WIN32
#define Shaderc_Command(_args) Process_Run("shadercRelease.exe", _args)
#else
#define Shaderc_Command(_args) Wasm_Run("shadercRelease.exe", _args)
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
        .Take(0)
        .Ref(varyings)
        // also fill the hash data sequence
        .Push(shader_hashing)
        .Input()
        .Take(1)
        .Ref(shader_code)
        // also fill the hash data sequence
        .Push(shader_hashing)
        .Input()
        .Take(2)
        .Ref(shader_type)
        // also fill the hash data sequence
        .Push(shader_hashing)
        .Input()
        .Take(3)
        .Ref(defines)
        // also fill the hash data sequence
        .Push(shader_hashing)
        // write temporary files
        .let("varying.def.sc")
        .FS_Write_Overwriting(varyings)
        .let("shader-tmp.txt")
        .FS_Write_Overwriting(shader_code)
        // populate command arguments
        .let("-f")
        .Push(args)
        .let("shader-tmp.txt")
        .Push(args)
        .let("-o")
        .Push(args)
        .let("shader-tmp.bin")
        .Push(args)
        .let("--platform")
        .Push(args)
        .Platform_Name()
        .Push(args)
        .let("-p")
        .Push(args)
        .Shader_Model()
        .Push(args)
        .let("--type")
        .Push(args)
        .Get(shader_type)
        .Push(args)
        // hash the shader code and other parameters
        .Get(shader_hashing)
        .Hash()
        .ToHex()
        .ToString()
        .Set(shader_hash_filename)
        .let("shaders_cache/")
        .PrependTo(shader_hash_filename)
        .Get(shader_hash_filename)
        .Maybe(FS_Read_Bytes(), Shaderc_Command(args)
                                    // read the temporary binary file
                                    .let("shader-tmp.bin")
                                    .FS_Read_Bytes()
                                    .Ref(shader_bytecode)
                                    .Get(shader_hash_filename)
                                    .FS_Write_Overwriting(shader_bytecode));

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
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

#if defined(__EMSCRIPTEN__)
#define Shader_Model() let("300_es")
#elif defined(BGFX_CONFIG_RENDERER_OPENGL) || defined(__linux__)
#if (BGFX_CONFIG_RENDERER_OPENGL_MIN_VERSION == 33)
// headless tests run at version 150 due to xvfb limitations
#define Shader_Model() let("150")
#else
#define Shader_Model() let("430")
#endif
#elif defined(__APPLE__)
#define Shader_Model() let("metal")
#elif defined(_WIN32)
#define Shader_Model()                                                         \
  Get(shader_type)                                                             \
      .If(Is("v"), let("vs_5_0"), If(Is("f"), let("ps_5_0"), let("cs_5_0")))
#endif

#ifdef _WIN32
#define Shaderc_Command(_args)                                                 \
  let("").Process_Run("shaders/shadercRelease.exe", _args)
#elif defined(__EMSCRIPTEN__)
#define Shaderc_Command(_args)                                                 \
  Get(varyings) SetTable(_args_em, "varyings")                                 \
      .Get(shader_code) SetTable(_args_em, "shader_code")                      \
      .Get(_args) SetTable(_args_em, "params")                                 \
      .Get(_args_em)                                                           \
      .ToJson()                                                                \
      .block("_Emscripten.CompileShader")
#else
#define Shaderc_Command(_args)                                                 \
  let("").Wasm_Run("shaders/shadercRelease.wasm", _args)
#endif

#ifndef NDEBUG
#define Verbose() let("--verbose") Push(args).let("--disasm") Push(args)
#else
#define Verbose() Pass()
#endif

chainblocks::Var empty_bytes((uint8_t *)nullptr, 0);

#ifdef __EMSCRIPTEN__
/*
Sadly out WASI shader compiler fails (call stack overflow)
So we need to use an emscripten module and a bit of JS for now
*/
// clang-format off
// not sure if it's a bug but this can only return "number" or false
EM_JS(char*, cb_emscripten_compile_shader, (const char *json), {
  return Asyncify.handleAsync(async () => {
    try {
      const paramsStr = UTF8ToString(json);
      const params = JSON.parse(paramsStr);

      // load the JS part if needed
      if (globalThis.shaderc === undefined) {
        // cache it at worker level
        importScripts("shaderc.js");
      }
      // also cache and load the wasm binary
      if (globalThis.shaderc_binary === undefined) {
        const response = await fetch("shaderc.wasm");
        const buffer = await response.arrayBuffer();
        globalThis.shaderc_binary = new Uint8Array(buffer);
      }

      var output = {
        stdout: "",
        stderr: ""
      };
      const compiler = await shaderc({
        noInitialRun: true,
        wasmBinary: shaderc_binary,
        print: function(text) {
          output.stdout += text;
        },
        printErr: function(text) {
          output.stderr += text;
        }
      });

      // shaders library
      compiler.FS.mkdir("/shaders/");
      compiler.FS.mkdir("/shaders/include");
      compiler.FS.mkdir("/shaders/lib");
      compiler.FS.mkdir("/shaders/lib/gltf");
      compiler.FS.mkdir("/shaders/cache");
      compiler.FS.mkdir("/shaders/tmp");

      var fetches = [];
      fetches.push({
        filename: "/shaders/include/bgfx_shader.h",
        operation: fetch("shaders/include/bgfx_shader.h")
      });
      fetches.push({
        filename: "/shaders/include/shaderlib.h",
        operation: fetch("shaders/include/shaderlib.h")
      });
      fetches.push({
        filename: "/shaders/lib/gltf/ps_entry.h",
        operation: fetch("shaders/lib/gltf/ps_entry.h")
      });
      fetches.push({
        filename: "/shaders/lib/gltf/vs_entry.h",
        operation: fetch("shaders/lib/gltf/vs_entry.h")
      });
      fetches.push({
        filename: "/shaders/lib/gltf/varying.h",
        operation: fetch("shaders/lib/gltf/varying.h")
      });

      for(let i = 0; i < fetches.length; i++) {
        const response = await fetches[i].operation;
        const buffer = await response.arrayBuffer();
        const view = new Uint8Array(buffer);
        compiler.FS.writeFile(fetches[i].filename, view);
      }

      // write the required files
      compiler.FS.writeFile("/shaders/tmp/shader.txt", params.shader_code);
      compiler.FS.writeFile("/shaders/tmp/varying.txt", params.varyings);

      // run the program until the end
      compiler.callMain(params.params);

      output.bytecode = Array.from(compiler.FS.readFile("/shaders/tmp/shader.bin"));

      const result = JSON.stringify(output);
      const len = lengthBytesUTF8(result) + 1;
      var obuffer = _malloc(len);
      stringToUTF8(result, obuffer, len);
      return obuffer;
    } catch(e) {
      console.error(e);
      return 0;
    }
  });
});
// clang-format on

namespace chainblocks {
CBVar emCompileShader(const CBVar &input) {
  static thread_local std::string str;
  auto res = cb_emscripten_compile_shader(input.payload.stringValue);
  const auto check = reinterpret_cast<intptr_t>(res);
  if (check == -1) {
    throw ActivationError(
        "Exception while compiling a shader, check the JS console");
  }

  MAIN_THREAD_EM_ASM(
      {
        const data = JSON.parse(UTF8ToString($0));
        const buffer = new Uint8Array(data.bytecode);
        FS.writeFile("shaders/tmp/shader.bin", buffer);
      },
      res);

  str.clear();
  if (res) {
    str.assign(res);
    free(res);
  }
  return Var(str);
}

using EmscriptenShaderCompiler =
    LambdaBlock<emCompileShader, CoreInfo::StringType, CoreInfo::StringType>;
void registerEmscriptenShaderCompiler() {
  REGISTER_CBLOCK("_Emscripten.CompileShader", EmscriptenShaderCompiler);
}
} // namespace chainblocks
#endif

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
                .Verbose()
                .Shaderc_Command(args)
                .Log()
                // read the temporary binary file result
                .let("shaders/tmp/shader.bin")
                .FS_Read_Bytes() Ref(shader_bytecode)
                .Brotli_Compress() Ref(shader_bytecode_compressed)
                .Get(shader_hash_filename)
                .FS_Write_Overwriting(shader_bytecode_compressed)
                .Get(shader_bytecode));

    _chain = shader_compiler;
  } // namespace chainblocks

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
    std::array<Var, 4> args = {v_varyings, v_code, v_type, v_defines};
    Var v_args{args};
    auto node = CBNode::make();
    LOG(DEBUG) << "Shader compiler about to be scheduled.";
    node->schedule(_chain, v_args);
    LOG(DEBUG) << "Shader compiler chain scheduled and ready to run.";
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
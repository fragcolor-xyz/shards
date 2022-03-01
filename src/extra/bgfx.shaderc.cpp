/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

// collection of runtime chains to allow shader compilation, caching and
// storage.

#include "./bgfx.hpp"
#include "nlohmann/json.hpp"
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
#elif defined(BGFX_CONFIG_RENDERER_VULKAN)
#define Shader_Model() let("spirv15-12")
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
#define Shader_Model() Get(shader_type).If(Is("v"), let("vs_5_0"), If(Is("f"), let("ps_5_0"), let("cs_5_0")))
#endif

#ifdef _WIN32
#define Shaderc_Command(_args) let("").Process_Run("shaders/shadercRelease.exe", _args)
#elif defined(__EMSCRIPTEN__)
#define Shaderc_Command(_args)                            \
  Get(varyings) SetTable(_args_em, "varyings")            \
      .Get(shader_code) SetTable(_args_em, "shader_code") \
      .Get(_args) SetTable(_args_em, "params")            \
      .Get(_args_em)                                      \
      .ToJson()                                           \
      .block("_Emscripten.CompileShader")
#else
#define Shaderc_Command(_args) let("").Wasm_Run("shaders/shadercRelease.wasm", _args)
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

extern "C" {
void emFetchShadersLibrary();
void emSetupShaderCompiler();
// not sure if it's a bug but this can only return "number" or false
// this implementation is blocking on the C side, promises and async did not
// work well when inside a worker
char *emCompileShaderBlocking(const char *json);
}

namespace chainblocks {
struct EmscriptenShaderCompiler {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    static thread_local std::string str;

    emSetupShaderCompiler();

#ifndef __EMSCRIPTEN_PTHREADS__
    const auto cb = emscripten::val::global("chainblocks");
    const auto compiler = cb["compileShaderFromJson"];
    const auto sres =
        emscripten_wait<emscripten::val>(context, compiler(emscripten::val(input.payload.stringValue))).as<std::string>();
    auto res = sres.c_str();
#else
    auto res = emCompileShaderBlocking(input.payload.stringValue);
    const auto check = reinterpret_cast<intptr_t>(res);
    if (check == -1) {
      throw ActivationError("Exception while compiling a shader, check the JS console");
    }
    DEFER(free(res));
#endif

    nlohmann::json j = nlohmann::json::parse(res);

    const auto err = j["stderr"].get<std::string>();
    if (err.size() > 0) {
      CBLOG_ERROR(err);
    }

    if (j.contains("bytecode")) {
      MAIN_THREAD_EM_ASM(
          {
            const data = JSON.parse(UTF8ToString($0));
            const buffer = new Uint8Array(data.bytecode);
            FS.writeFile("shaders/tmp/shader.bin", buffer);
          },
          res);
    } else {
      CBLOG_INFO(j["stdout"].get<std::string>());
      throw ActivationError("Failed to compile a shader.");
    }

    str.assign(j["stdout"].get<std::string>());
    if (str.size() == 0) {
      str.assign("Successfully compiled a shader.");
    }

    return Var(str);
  }
};

void registerEmscriptenShaderCompiler() {
  // this is good for the single threaded path
  emSetupShaderCompiler();
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
        .ToBytes()
        .ToBase58() Set(shader_hash_filename)
        .let("shaders/cache/") PrependTo(shader_hash_filename) //
        .Get(shader_hash_filename)
        .MaybeSilently( // try to load from cache
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

  void warmup(CBContext *context) override {
#ifdef __EMSCRIPTEN__
    emFetchShadersLibrary();
    const auto cb = emscripten::val::global("chainblocks");
    const auto fetch = cb["emFetchShadersLibraryAsync"];
    emscripten_wait<emscripten::val>(context, fetch());
#endif
  }

  CBVar compile(std::string_view varyings, //
                std::string_view code,     //
                std::string_view type,     //
                std::string_view defines,  //
                CBContext *context         //
                ) override {
    Var v_varyings{varyings.data(), varyings.size()};
    Var v_code{code.data(), code.size()};
    Var v_type{type.data(), type.size()};
    Var v_defines{defines.data(), defines.size()};
    std::array<Var, 4> args = {v_varyings, v_code, v_type, v_defines};
    Var v_args{args};

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    // We can make some assumptions...
    // 1 the caller even if inside an await, will still be in the main thread...
    // so using the context to suspend is safe in this case
    auto node = context->main->node.lock();
    node->schedule(_chain, v_args);
    while (isRunning(_chain.get())) {
      CB_SUSPEND(context, 0);
    }
#else
    auto node = CBNode::make();
    CBLOG_DEBUG("Shader compiler about to be scheduled");
    node->schedule(_chain, v_args);
    CBLOG_DEBUG("Shader compiler chain scheduled and ready to run");
    while (!node->empty()) {
      if (!node->tick())
        break;
    }
    auto errors = node->errors();
    if (errors.size() > 0) {
      for (auto &error : errors) {
        CBLOG_ERROR(error);
      }
      throw CBException("Shader compiler failed to compile, check errors above.");
    }
#endif

    if (_chain->finishedOutput.valueType != CBType::Bytes) {
      throw CBException("Shader compiler failed to compile, no output.");
    }
    return _chain->finishedOutput;
  }

private:
  chainblocks::Chain _chain;
};

std::unique_ptr<IShaderCompiler> makeShaderCompiler() { return std::make_unique<ShaderCompiler>(); }
} // namespace chainblocks
/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

// emscripten related utilities

mergeInto(LibraryManager.library, {
  emFetchShadersLibraryAsync: async function () {
    if (globalThis.chainblocks === undefined) {
      globalThis.chainblocks = {
        shaderCompilerSetup: false
      };
    }

    if (!globalThis.chainblocks.shaderCompilerSetup) {
      try {
        if (globalThis.chainblocks.shadercBinary === undefined) {
          const response = await fetch("shaders/shaderc.wasm");
          const buffer = await response.arrayBuffer();
          globalThis.chainblocks.shadercBinary = new Uint8Array(buffer);
        }

        // load the JS part if needed
        if (globalThis.shaderc === undefined) {
          if (typeof importScripts === 'function') {
            // when inside a worker (we are using threads)
            // cache it at worker level
            importScripts("shaders/shaderc.js");
          } else {
            var shadercLoaded = new Promise((resolve, _reject) => {
              const shaderc = document.createElement("script");
              shaderc.src = "shaders/shaderc.js";
              shaderc.async = true;
              shaderc.onload = async function () {
                resolve();
              };
              document.body.appendChild(shaderc);
            });
            await shadercLoaded;
          }
        }

        FS.mkdir("/shaders/");
        FS.mkdir("/shaders/include");
        FS.mkdir("/shaders/lib");
        FS.mkdir("/shaders/lib/gltf");
        FS.mkdir("/shaders/cache");
        FS.mkdir("/shaders/tmp");

        var fetches = [];
        // /include from our /include
        fetches.push({
          filename: "/shaders/include/shader.h",
          operation: fetch("shaders/include/shader.h")
        });
        fetches.push({
          filename: "/shaders/include/implicit_shapes.h",
          operation: fetch("shaders/include/implicit_shapes.h")
        });
        fetches.push({
          filename: "/shaders/include/noise.h",
          operation: fetch("shaders/include/noise.h")
        });
        fetches.push({
          filename: "/shaders/include/ShaderFastMathLib.h",
          operation: fetch("shaders/include/ShaderFastMathLib.h")
        });
        // /include from bgfx folders
        fetches.push({
          filename: "/shaders/include/bgfx_shader.h",
          operation: fetch("shaders/include/bgfx_shader.h")
        });
        fetches.push({
          filename: "/shaders/include/shaderlib.h",
          operation: fetch("shaders/include/shaderlib.h")
        });
        // /lib/gltf from our /gltf
        fetches.push({
          filename: "/shaders/lib/gltf/ps_entry.h",
          operation: fetch("shaders/lib/gltf/ps_entry.h")
        });
        fetches.push({
          filename: "/shaders/lib/gltf/vs_entry.h",
          operation: fetch("shaders/lib/gltf/vs_entry.h")
        });
        fetches.push({
          filename: "/shaders/lib/gltf/varying.txt",
          operation: fetch("shaders/lib/gltf/varying.txt")
        });

        for (let i = 0; i < fetches.length; i++) {
          const response = await fetches[i].operation;
          const buffer = await response.arrayBuffer();
          const view = new Uint8Array(buffer);
          FS.writeFile(fetches[i].filename, view);
        }

        globalThis.chainblocks.shaderCompilerSetup = true;
      } catch (e) {
        console.log(e);
        throw e;
      }
    }
  },
  emFetchShadersLibrary: function () {
    globalThis.chainblocks.emFetchShadersLibraryAsync = _emFetchShadersLibraryAsync;
    // return Asyncify.handleAsync(async () => {
    //   await _emFetchShadersLibraryAsync();
    //   return 0;
    // });
  },
  emFetchShadersLibrary__deps: ['emFetchShadersLibraryAsync'],
  emSetupShaderCompiler: function () {
    if (globalThis.chainblocks === undefined) {
      globalThis.chainblocks = {};
    }

    if (globalThis.chainblocks.compileShader === undefined) {
      globalThis.chainblocks.compileShader = async function (params) {
        await _emFetchShadersLibraryAsync();

        var output = {
          stdout: "",
          stderr: "",
          bytecode: null
        };

        const compiler = await shaderc({ // this must be preloaded
          noInitialRun: true,
          wasmBinary: globalThis.chainblocks.shadercBinary,
          print: function (text) {
            output.stdout += text;
          },
          printErr: function (text) {
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
        // /include from our /include
        fetches.push({
          filename: "/shaders/include/shader.h",
          operation: fetch("shaders/include/shader.h")
        });
        fetches.push({
          filename: "/shaders/include/implicit_shapes.h",
          operation: fetch("shaders/include/implicit_shapes.h")
        });
        fetches.push({
          filename: "/shaders/include/noise.h",
          operation: fetch("shaders/include/noise.h")
        });
        fetches.push({
          filename: "/shaders/include/ShaderFastMathLib.h",
          operation: fetch("shaders/include/ShaderFastMathLib.h")
        });
        // /include from bgfx folders
        fetches.push({
          filename: "/shaders/include/bgfx_shader.h",
          operation: fetch("shaders/include/bgfx_shader.h")
        });
        fetches.push({
          filename: "/shaders/include/shaderlib.h",
          operation: fetch("shaders/include/shaderlib.h")
        });
        // /lib/gltf from our /gltf
        fetches.push({
          filename: "/shaders/lib/gltf/ps_entry.h",
          operation: fetch("shaders/lib/gltf/ps_entry.h")
        });
        fetches.push({
          filename: "/shaders/lib/gltf/vs_entry.h",
          operation: fetch("shaders/lib/gltf/vs_entry.h")
        });
        fetches.push({
          filename: "/shaders/lib/gltf/varying.txt",
          operation: fetch("shaders/lib/gltf/varying.txt")
        });

        for (let i = 0; i < fetches.length; i++) {
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

        const fileExists = compiler.FS.analyzePath("/shaders/tmp/shader.bin").exists;
        // if exists should be successful
        if (fileExists) {
          output.bytecode = Array.from(compiler.FS.readFile("/shaders/tmp/shader.bin"));
        }

        return output;
      };
    }

    if (globalThis.chainblocks.compileShaderFromJson === undefined) {
      globalThis.chainblocks.compileShaderFromJson = async function (json) {
        await _emFetchShadersLibraryAsync();

        const params = JSON.parse(json);

        const output = await globalThis.chainblocks.compileShader(params);

        return JSON.stringify(output);
      };
    }

    if (globalThis.chainblocks.compileShaderFromObject === undefined) {
      globalThis.chainblocks.compileShaderFromObject = async function (params) {
        await _emFetchShadersLibraryAsync();

        const output = await globalThis.chainblocks.compileShader(params);

        return JSON.stringify(output);
      };
    }
  },
  emCompileShaderBlocking: function (json) {
    return Asyncify.handleAsync(async () => {
      try {
        const paramsStr = UTF8ToString(json);
        const result = await globalThis.chainblocks.compileShaderFromJson(paramsStr);
        const len = lengthBytesUTF8(result) + 1;
        var obuffer = _malloc(len);
        stringToUTF8(result, obuffer, len);
        return obuffer;
      } catch (e) {
        console.error(e);
        return 0;
      }
    });
  }
});
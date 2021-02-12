/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

// emscripten related utilities

mergeInto(LibraryManager.library, {
  emSetupShaderCompiler: function () {
    if (globalThis.chainblocks === undefined) {
      globalThis.chainblocks = {};
    }

    if (globalThis.chainblocks.compileShader === undefined) {
      globalThis.chainblocks.compileShader = async function (params) {
        var output = {
          stdout: "",
          stderr: ""
        };

        const compiler = await shaderc({ // this must be preloaded
          noInitialRun: true,
          wasmBinary: shaderc_binary, // this must be preloaded
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
  },
  emCompileShaderBlocking: function (json) {
    return Asyncify.handleAsync(async () => {
      try {
        const paramsStr = UTF8ToString(json);
        const params = JSON.parse(paramsStr);

        // load the JS part if needed
        if (globalThis.shaderc === undefined) {
          // when inside a worker (we are using threads)
          // cache it at worker level
          importScripts("shaderc.js");
        }

        // also cache and load the wasm binary
        if (globalThis.shaderc_binary === undefined) {
          const response = await fetch("shaderc.wasm");
          const buffer = await response.arrayBuffer();
          globalThis.shaderc_binary = new Uint8Array(buffer);
        }

        const output = await globalThis.chainblocks.compileShader(params);

        const result = JSON.stringify(output);
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
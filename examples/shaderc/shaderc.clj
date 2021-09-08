; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2021 Fragcolor Pte. Ltd.

(def shaders-platform
  (cond
    (= platform "windows") "windows"
    (= platform "apple") "ios"
    (or
     (= platform "linux")
     (= platform "emscripten")) "linux"))

(def shaders-profile
  (cond
    (= platform "windows") "s_5"
    (= platform "apple") "metal"
    (= platform "linux") "450"
    (= platform "emscripten") ""))

(defn shaderc [type in out]
  (if (= platform "windows")
    ~[(Process.Run
       "shadercRelease.exe"
       ["-f" in
        "-o" out
        "--platform" shaders-platform
        "-p" shaders-profile
        "--type" type])]
    ~[(Wasm.Run
       "shadercRelease.wasm"
       ["-f" in
        "-o" out
        "--platform" shaders-platform
        "-p" shaders-profile
        "--type" type])]))

(defn Shader [varying vs-code fs-code] 
  (let [loader 
        (Chain
         "shader-compiler"
         "varying.def.sc"
         (FS.Write varying :Overwrite true)
         "vs-shader-tmp.txt"
         (FS.Write vs-code :Overwrite true)
         "fs-shader-tmp.txt"
         (FS.Write fs-code :Overwrite true)
         (shaderc "v" "vs-shader-tmp.txt" "vs-bytecode-tmp.bin")
         (shaderc "f" "fs-shader-tmp.txt" "fs-bytecode-tmp.bin")
         "vs-bytecode-tmp.bin"
         (FS.Read :Bytes true) >> .bytecodes
         "fs-bytecode-tmp.bin"
         (FS.Read :Bytes true) >> .bytecodes
         .bytecodes)
        node (Node)]
    (schedule node loader)
    (run node 0.1)
    ; read results and cleanup the chain
    (stop loader)))

(prn (Shader
      ; varying info
      "
vec4 v_color0   : COLOR0 = vec4(1.0, 0.0, 0.0, 1.0);
vec3 a_position : POSITION;
vec4 a_color0   : COLOR0;
"
      ; vertex shader
      "
$input a_position, a_color0
$output v_color0

#include <bgfx_shader.sh>

void main() {
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_color0 = a_color0;
}
"
      ; fragment shader
      "
$input v_color0

#include <bgfx_shader.sh>

void main() {
	gl_FragColor = v_color0;
}
"))
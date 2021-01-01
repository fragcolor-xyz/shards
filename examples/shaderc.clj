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
    (= platform "emscripten")) "")

(defn shaderc [type in out]
  (if (= platform "windows")
    ~[(Process.Run "shadercRelease.exe")]
    ~[(Wasm.Run
       "shadercRelease.wasm" ["-f" in
                              "-o" out
                              "--platform" shaders-platform
                              "-p" shaders-profile
                              "--type" type])]))

(defn Shader [vs-code fs-code] 
  (let [loader 
        (Chain
         "shader-compiler"
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
    ; read results
    (stop loader)))

(prn (Shader "" ""))
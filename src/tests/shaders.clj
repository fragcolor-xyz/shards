; example showing how to use a wasm tool to compile a shader and load it

(def Root (Node))

(def loadShader
  (Chain
   "loadShader"
   (Input) (Take 0) >= .infile >= .outfile
   ".bin" (AppendTo .outfile)
   "-f" >> .shadercArgs .infile >> .shadercArgs
   "-o" >> .shadercArgs .outfile >> .shadercArgs
   "--type" >> .shadercArgs (Input) (Take 1) >> .shadercArgs
   "-p" >> .shadercArgs "spirv" >> .shadercArgs
   "--platform" >> .shadercArgs "windows" >> .shadercArgs
   "-i" >> .shadercArgs "../../deps/bgfx/src" >> .shadercArgs
   "--verbose" >> .shadercArgs
   "" (Wasm.Run "../../external/shadercRelease.wasm" .shadercArgs)
   (Log "shaderc")))

(def main
  (Chain
   "main"
   "../../assets/basic.vs.glsl" >> .args
   "v" >> .args
   .args (Do loadShader)))

(schedule Root main)
(run Root 0.1)

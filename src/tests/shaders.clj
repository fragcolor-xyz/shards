; example showing how to use a wasm tool to compile a shader and load it

(def Root (Node))

(def loadShader
  (Chain
   "loadShader"
   (Input) (Take 0) >= .infile >= .outfile
   ".bin" (AppendTo .outfile)
   .outfile (Log "writing")
   "-f" >> .shadercArgs .infile >> .shadercArgs
   "-o" >> .shadercArgs .outfile >> .shadercArgs
   "--type" >> .shadercArgs (Input) (Take 1) >> .shadercArgs
  ;;  "-p" >> .shadercArgs "450" >> .shadercArgs
   "--platform" >> .shadercArgs "asm.js" >> .shadercArgs
   "-i" >> .shadercArgs "../../deps/bgfx/src" >> .shadercArgs
   "--verbose" >> .shadercArgs
   "" (Wasm.Run "../../external/shadercRelease.wasm"
                .shadercArgs
                :ResetRuntime true) (Log "shaderc")
   .outfile
   (FS.Read :Bytes true)))

(def main
  (Chain
   "main"
  ;;  (GFX.MainWindow :Title "SDL Window" :Width 1024 :Height 1024)

   "../../deps/bgfx/examples/01-cubes/vs_cubes.sc" >> .args
   "v" >> .args
   .args (Do loadShader) >= .vshader

   (Clear .args)
   "../../deps/bgfx/examples/01-cubes/fs_cubes.sc" >> .args
   "f" >> .args
   .args (Do loadShader) >= .pshader

  ;;  (GFX.Shader .vshader .pshader)

  ;;  (GFX.Draw)
   ))

(schedule Root main)
(run Root 0.1)

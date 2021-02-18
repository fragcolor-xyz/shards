(def! Root (Node))

(def cube
  {"Vertices"
   [(Float3 -1.0  1.0  1.0) 0xff000000
    (Float3  1.0  1.0  1.0) 0xff0000ff
    (Float3 -1.0 -1.0  1.0) 0xff00ff00
    (Float3  1.0 -1.0  1.0) 0xff00ffff
    (Float3 -1.0  1.0 -1.0) 0xffff0000
    (Float3  1.0  1.0 -1.0) 0xffff00ff
    (Float3 -1.0 -1.0 -1.0) 0xffffff00
    (Float3  1.0 -1.0 -1.0) 0xffffffff]
   "Indices"
   [(Int3 0 1 2)
    (Int3 1 3 2)
    (Int3 4 6 5)
    (Int3 5 6 7)
    (Int3 0 2 4)
    (Int3 4 2 6)
    (Int3 1 5 3)
    (Int3 5 7 3)
    (Int3 0 4 1)
    (Int3 4 5 1)
    (Int3 2 3 6)
    (Int3 6 3 7)]})

(def identity
  [(Float4 1 0 0 0)
   (Float4 0 1 0 0)
   (Float4 0 0 1 0)
   (Float4 0 0 0 1)])

(def shaders-folder
  (cond
    (= platform "windows") "dx11"
    (= platform "apple") "metal"
    (or 
     (= platform "linux") 
     (= platform "emscripten")) "glsl"))

(def test-chain
  (Chain
   "test-chain"
   :Looped
   (GFX.MainWindow
    :Title "SDL Window" :Width 1024 :Height 1024 :Debug true
    :Contents
    ~[(Once
       ~[(LoadImage "../../assets/drawing.png")
         (GFX.Texture2D) >= .image1
         (LoadImage "../../deps/bgfx/examples/06-bump/fieldstone-rgba.tga")
         (GFX.Texture2D :sRGB true) >> .bump-textures
         (LoadImage "../../deps/bgfx/examples/06-bump/fieldstone-n.tga")
         (GFX.Texture2D) >> .bump-textures
         cube (GFX.Model
               :Layout [VertexAttribute.Position
                        VertexAttribute.Color0]
               :CullMode CullMode.Front) >= .cube
         (str "../../deps/bgfx/examples/runtime/shaders/" shaders-folder "/vs_cubes.bin")
         (FS.Read :Bytes true) >= .vs_bytes
         (str "../../deps/bgfx/examples/runtime/shaders/" shaders-folder "/fs_cubes.bin")
         (FS.Read :Bytes true) >= .fs_bytes
         (GFX.Shader :VertexShader .vs_bytes
                     :PixelShader .fs_bytes) >= .shader
         (str "../../deps/bgfx/examples/runtime/shaders/" shaders-folder "/vs_bump.bin")
         (FS.Read :Bytes true) > .vs_bytes
         (str "../../deps/bgfx/examples/runtime/shaders/" shaders-folder "/fs_bump.bin")
         (FS.Read :Bytes true) > .fs_bytes
         (GFX.Shader :VertexShader .vs_bytes
                     :PixelShader .fs_bytes) >= .bump-shader
         false (Set "checkBoxie")
         (Inputs.Mouse :Hidden true :Capture true :Relative true)
         (Physics.Ball 0.5) = .ball-pshape
         (Physics.Cuboid (Float3 100 1 100)) = .ground-pshape])
      ; regular model render
      {"Position" (Float3 0 0 10)
       "Target" (Float3 0 0 0)} (GFX.Camera)
      [(Float4 1.0 0.7 0.2 0.8)
       (Float4 0.7 0.2 1.0 0.8)
       (Float4 0.2 1.0 0.7 0.8)
       (Float4 1.0 0.4 0.2 0.8)] (GFX.SetUniform "u_lightPosRadius" 4)
      [(Float4 1.0 0.7 0.2 0.8)
       (Float4 0.7 0.2 1.0 0.8)
       (Float4 0.2 1.0 0.7 0.8)
       (Float4 1.0 0.4 0.2 0.8)] (GFX.SetUniform "u_lightRgbInnerR" 4)
      identity (GFX.Draw :Shader .shader :Model .cube)
      (Physics.Simulation)
      (Physics.StaticRigidBody .ground-pshape (Float3 0.0 -5.0 0.0))
      (Physics.DynamicRigidBody .ball-pshape) (Log "rb")
      (GUI.Window :Title "My ImGui" :Width 1024 :Height 1024
                  :PosX 0 :PosY 0 :Contents
                  (->
                   "Hello world"   (GUI.Text)
                   "Hello world 2" (GUI.Text)
                   "Hello world 3" (GUI.SameLine) (GUI.Text)
                   "Hello world 4" (GUI.SameLine) (GUI.Text)
                   (Inputs.MousePos) (GUI.Text "mouse pos")
                   (Inputs.MouseDelta) (GUI.Text "mouse delta")
                   (GUI.Separator)

                   (GUI.Indent)
                   99 (GUI.Text "Player1" (Color 255 0 0 255))
                   (GUI.Unindent)
                   99 (GUI.Text "Player1")
                   99 (GUI.Text "Player1")
                   (GUI.Separator)
                   (GUI.CheckBox "CheckBoxie" "checkBoxie") (GUI.SameLine) (GUI.Text)
                   (Float4 2 3 4 5)
                   (Set "x")
                   (Float4 1 2 3 4)
                   (Math.Add .x)
                   (GUI.Text)

                   (Get "image1")
                   (GUI.Image (Float2 0.1 0.1))

                   (GFX.RenderTexture 256 256
                                      (->
                                       ; render the model
                                       {"Position" (Float3 5 5 5)
                                        "Target" (Float3 0 0 0)} (GFX.Camera)
                                       ; this model is not really idea, vertex layout is not correct
                                       identity (GFX.Draw :Shader .bump-shader
                                                          :Textures .bump-textures
                                                          :Model .cube)))
                   (GUI.Image (Float2 1.0 1.0))

                   (GUI.SameLine)

                   (GFX.RenderTexture 64 64
                                      (->
                                       ; full screen quad pass
                                       nil (GFX.CameraOrtho) ; to set only projection
                                       ; no model means full screen quad
                                       identity (GFX.Draw :Shader .shader)))
                   (GUI.Image (Float2 1.0 1.0))

                   (GFX.RenderTexture 64 64
                                      (->
                                       ; render the model
                                       {"Position" (Float3 0 0 2)
                                        "Target" (Float3 0 0 0)} (GFX.Camera)
                                       ; this model is not really idea, vertex layout is not correct
                                       identity (GFX.Draw :Shader .shader
                                                          :Model .cube)))
                   (GUI.Image (Float2 1.0 1.0))

                   (GUI.ChildWindow
                    :Border true :Contents
                    #((GUI.TreeNode
                       "Node1"
                       (->
                        "Node text..."
                        (GUI.Text)
                        (GUI.TextInput "Say something" "text1")
                        (GUI.Text "<-- you said!")

                        (GUI.IntInput)
                        (GUI.Text)

                        (GUI.FloatInput)
                        (GUI.Text)

                        (GUI.Int3Input)
                        (GUI.Text)

                        (GUI.Float3Input "f3" "f3var")
                        (Get "f3var")
                        (GUI.Text)

                        (GUI.FloatDrag)
                        (GUI.Text)

                        (GUI.ColorInput)
                        (GUI.Text)

                        (GUI.Float3Drag)
                        (GUI.Text)
                        (GUI.Plot "Plot"
                                  #((Const [(Float2 10 3) (Float2 5 6) (Float2 9 10)])
                                    (GUI.PlotLine)
                                    (GUI.PlotDigital)
                                    (Math.Add (Float2 -10 -10))
                                    (GUI.PlotScatter)
                                    (Math.Add (Float2 5 3))
                                    (GUI.PlotBars))
                                  :X_Limits (Float2 -10 10)
                                  :Y_Limits (Float2 -10 10)
                                  :Lock_X false
                                  :Lock_Y true)))))

                   (GUI.Button "Push me!" (->
                                           (Msg "Action!")))
                   (Cond [(-> (Is true)) (-> (Msg "yeah..."))])
                   (GUI.Button "Push me!" (->
                                           (Msg "Action!")) ImGuiButton.Small)
                   (Cond [(-> (Is true)) (-> (Msg "yeah..."))])
                   (GUI.Button "Push me!" (->
                                           (Msg "Action!")) ImGuiButton.ArrowUp)
                   (Cond [(-> (Is true)) (-> (Msg "yeah..."))])
                   (GUI.ChildWindow :Border true :Width 100 :Height 100 :Contents
                                    #((ToBytes)
                                      (GUI.HexViewer)))))])))

(schedule Root test-chain)
(run Root 0.02 100)
;; (run Root 0.02)

(schedule Root test-chain)
(run Root 0.02 100)

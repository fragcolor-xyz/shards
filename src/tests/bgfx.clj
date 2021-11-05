; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

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

(def pos1
  [(Float4 1 0 0 0)
   (Float4 0 1 0 0)
   (Float4 0 0 1 0)
   (Float4 0 2 0 1)])

(def shaders-folder
  (cond
    (= platform "windows") "dx11"
    (= platform "apple") "metal"
    (or
     (= platform "linux")
     (= platform "emscripten")) "glsl"))

(defn FColor
  [r g b a]
  (Color (* 255 r) (* 255 g) (* 255 b) (* 255 a)))

(defn applyStyle []
  (-> (Float2 15 15) (GUI.Style GuiStyle.WindowPadding)
      5.0 (GUI.Style GuiStyle.WindowRounding)
      (Float2 5 5) (GUI.Style GuiStyle.FramePadding)
      (Float2 12 8) (GUI.Style GuiStyle.ItemSpacing)
      (Float2 8 6) (GUI.Style GuiStyle.ItemInnerSpacing)
      25.0 (GUI.Style GuiStyle.IndentSpacing)
      15.0 (GUI.Style GuiStyle.ScrollbarSize)
      9.0 (GUI.Style GuiStyle.ScrollbarRounding)
      5.0 (GUI.Style GuiStyle.GrabMinSize)
      3.0 (GUI.Style GuiStyle.GrabRounding)
      (FColor 0.80 0.80 0.83 1.00) (GUI.Style GuiStyle.TextColor)
      (FColor 0.24 0.23 0.29 1.00) (GUI.Style GuiStyle.TextDisabledColor)
      (FColor 0.06 0.05 0.07 1.00) (GUI.Style GuiStyle.WindowBgColor)
      (FColor 0.07 0.07 0.09 1.00) (GUI.Style GuiStyle.ChildBgColor)
      (FColor 0.07 0.07 0.09 1.00) (GUI.Style GuiStyle.PopupBgColor)
      (FColor 0.80 0.80 0.83 0.88) (GUI.Style GuiStyle.BorderColor)
      (FColor 0.92 0.91 0.88 0.00) (GUI.Style GuiStyle.BorderShadowColor)
      (FColor 0.10 0.09 0.12 1.00) (GUI.Style GuiStyle.FrameBgColor)
      (FColor 0.24 0.23 0.29 1.00) (GUI.Style GuiStyle.FrameBgHoveredColor)
      (FColor 0.56 0.56 0.58 1.00) (GUI.Style GuiStyle.FrameBgActiveColor)
      (FColor 0.10 0.09 0.12 1.00) (GUI.Style GuiStyle.TitleBgColor)
      (FColor 1.00 0.98 0.95 0.75) (GUI.Style GuiStyle.TitleBgCollapsedColor)
      (FColor 0.07 0.07 0.09 1.00) (GUI.Style GuiStyle.TitleBgActiveColor)
      (FColor 0.10 0.09 0.12 1.00) (GUI.Style GuiStyle.MenuBarBgColor)
      (FColor 0.10 0.09 0.12 1.00) (GUI.Style GuiStyle.ScrollbarBgColor)
      (FColor 0.80 0.80 0.83 0.31) (GUI.Style GuiStyle.ScrollbarGrabColor)
      (FColor 0.56 0.56 0.58 1.00) (GUI.Style GuiStyle.ScrollbarGrabHoveredColor)
      (FColor 0.06 0.05 0.07 1.00) (GUI.Style GuiStyle.ScrollbarGrabActiveColor)
      (FColor 0.80 0.80 0.83 0.31) (GUI.Style GuiStyle.CheckMarkColor)
      (FColor 0.80 0.80 0.83 0.31) (GUI.Style GuiStyle.SliderGrabColor)
      (FColor 0.06 0.05 0.07 1.00) (GUI.Style GuiStyle.SliderGrabActiveColor)
      (FColor 0.10 0.09 0.12 1.00) (GUI.Style GuiStyle.ButtonColor)
      (FColor 0.24 0.23 0.29 1.00) (GUI.Style GuiStyle.ButtonHoveredColor)
      (FColor 0.56 0.56 0.58 1.00) (GUI.Style GuiStyle.ButtonActiveColor)
      (FColor 0.10 0.09 0.12 1.00) (GUI.Style GuiStyle.HeaderColor)
      (FColor 0.56 0.56 0.58 1.00) (GUI.Style GuiStyle.HeaderHoveredColor)
      (FColor 0.06 0.05 0.07 1.00) (GUI.Style GuiStyle.HeaderActiveColor)
      (FColor 0.00 0.00 0.00 0.00) (GUI.Style GuiStyle.ResizeGripColor)
      (FColor 0.56 0.56 0.58 1.00) (GUI.Style GuiStyle.ResizeGripHoveredColor)
      (FColor 0.06 0.05 0.07 1.00) (GUI.Style GuiStyle.ResizeGripActiveColor)
      (FColor 0.40 0.39 0.38 0.63) (GUI.Style GuiStyle.PlotLinesColor)
      (FColor 0.25 1.00 0.00 1.00) (GUI.Style GuiStyle.PlotLinesHoveredColor)
      (FColor 0.40 0.39 0.38 0.63) (GUI.Style GuiStyle.PlotHistogramColor)
      (FColor 0.25 1.00 0.00 1.00) (GUI.Style GuiStyle.PlotHistogramHoveredColor)
      (FColor 0.25 1.00 0.00 0.43) (GUI.Style GuiStyle.TextSelectedBgColor)))

(def test-chain
  (Chain
   "test-chain"
   :Looped
   (GFX.MainWindow
    :Title "SDL Window" :Width 1024 :Height 1024 :Debug true :Fullscreen true
    :Contents
    ~[(Setup
       (applyStyle)
       (LoadImage "../../assets/drawing.png")
       (GFX.Texture2D) >= .image1
       (LoadImage "../../deps/bgfx/examples/06-bump/fieldstone-rgba.tga")
       (GFX.Texture2D) >> .bump-textures
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

       (str "../../deps/bgfx/examples/runtime/shaders/" shaders-folder "/vs_instancing.bin")
       (FS.Read :Bytes true) > .vs_bytes
       (str "../../deps/bgfx/examples/runtime/shaders/" shaders-folder "/fs_instancing.bin")
       (FS.Read :Bytes true) > .fs_bytes
       (GFX.Shader :VertexShader .vs_bytes
                   :PixelShader .fs_bytes) >= .instancing-shader

       false (Set "checkBoxie")
       (Inputs.Mouse :Hidden true :Capture true :Relative true)
       (Physics.Ball :Radius 0.5) = .ball-pshape
       (Physics.Cuboid :HalfExtents (Float3 1.0 1.0 1.0)) = .cube-pshape
       (Physics.Cuboid :HalfExtents (Float3 100 1 100)) = .ground-pshape)
      
      ; regular model render
      {"Position" (Float3 5 1 8)
       "Target" (Float3 0 0 0)} (GFX.Camera)
      
      [(Float4 1.0 0.7 0.2 0.8)
       (Float4 0.7 0.2 1.0 0.8)
       (Float4 0.2 1.0 0.7 0.8)
       (Float4 1.0 0.4 0.2 0.8)] (GFX.SetUniform "u_lightPosRadius" 4)
      [(Float4 1.0 0.7 0.2 0.8)
       (Float4 0.7 0.2 1.0 0.8)
       (Float4 0.2 1.0 0.7 0.8)
       (Float4 1.0 0.4 0.2 0.8)] (GFX.SetUniform "u_lightRgbInnerR" 4)
      (Physics.Simulation)
      (Physics.KinematicBody .ground-pshape (Float3 0.0 -4.0 0.0))
      ;; (Physics.DynamicBody .cube-pshape :Name "rb1")
      ;; (Once (-> .rb1 (Log)))
      ;; (GFX.Draw :Shader .shader :Model .cube :Blend {"Src" Blend.One "Dst" Blend.Zero "Op" BlendOp.Add})

      ; drop a few boxes
      (defblocks box [n]
        (Physics.DynamicBody .cube-pshape :Name (str "rb" n))
        (GFX.Draw :Shader .shader :Model .cube :Blend {"Src" Blend.One "Dst" Blend.Zero "Op" BlendOp.Add}))
      (map box (range 1 5))

      (defblocks onHover [n]
        (Float3 0.1 4 0)
        (Physics.Impulse (ContextVar (str "rb" n)))
        (str "Box " n))

      (GUI.Window :Title "My ImGui" :Width 1024 :Height 1024
                  :AllowResize true :AllowMove true :AllowCollapse true
                  :Pos (Int2 0 0) :Contents
                  (->
                   (Inputs.MousePos)
                   (| (GFX.Unproject 0.0) = .ray-from (GUI.Text "from"))
                   (| (GFX.Unproject 1.0) = .ray-to (GUI.Text "to"))
                   .ray-to (Math.Subtract .ray-from) (Math.Normalize) = .ray-dir
                   (GUI.Text "ray")
                   [.ray-from .ray-dir] (Physics.CastRay)
                   (Match [[.rb1] (onHover 1)
                           [.rb2] (onHover 2)
                           [.rb3] (onHover 3)
                           [.rb4] (onHover 4)
                           [.rb5] (onHover 5)
                           nil (-> "No Box")]
                          :Passthrough false) (GUI.Text)

                   (Window.Size) (GUI.Text)
                   "Hello world"   (GUI.Text)
                   "Hello world 2" (GUI.Text)
                   "Hello world 3" (GUI.SameLine) (GUI.Text)
                   "Hello world 4" (GUI.SameLine) (GUI.Text)
                  ;;  (Inputs.MousePos) (GUI.Text "mouse pos")
                  ;;  (| (GFX.Unproject 0.0) (GUI.Text "mouse pos world 0") = .ray-from)
                  ;;  (| (GFX.Unproject 1.0) (GUI.Text "mouse pos world 1") = .ray-to)
                  ;;  .ray-to (Math.Subtract .ray-from) = .ray-dir
                  ;;  [.ray-from .ray-dir] (Physics.CastRay)
                  ;;  (Match [[.rb1] (->
                  ;;                  (Float3 0 10 0) (Physics.Impulse .rb1)
                  ;;                  (Msg "Mouse over the box..."))
                  ;;          nil nil])
                   (Inputs.MouseDelta) (GUI.Text "mouse delta")
                   (GUI.Separator)

                   (GUI.Indent)
                   99 (GUI.Text "Player1" (Color 255 0 0 255))
                   (GUI.Unindent)
                   99 (GUI.Text "Player1")
                   99 (GUI.Text "Player1")
                   (GUI.Separator)
                   (GUI.Checkbox "CheckBoxie" .checkBoxie) (GUI.SameLine) (GUI.Text)
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
                                                          :Model .cube
                                                          :Blend [{"Src" Blend.One "Dst" Blend.Zero "Op" BlendOp.Add}
                                                                  {"Src" Blend.One "Dst" Blend.Zero "Op" BlendOp.Add}])))
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
                                       [identity pos1] (GFX.Draw :Shader .instancing-shader
                                                                 :Model .cube)))
                   (GUI.Image (Float2 1.0 1.0))

                   (GUI.ChildWindow :Height 150
                    :Border true :Contents
                    (-> (GUI.TreeNode
                         "Node1"
                         (->
                          "Node text..."
                          (GUI.Text)
                          (GUI.TextInput "Say something" .text1)
                          (GUI.Text "<-- you said!")

                          (GUI.IntInput)
                          (GUI.Text)

                          (GUI.FloatInput)
                          (GUI.Text)

                          (GUI.Int3Input)
                          (GUI.Text)

                          (GUI.Float3Input "f3" .f3var)
                          (Get .f3var)
                          (GUI.Text)

                          (GUI.FloatDrag)
                          (GUI.Text)

                          (GUI.ColorInput)
                          (GUI.Text)

                          (GUI.Float3Drag)
                          (GUI.Text)
                          (GUI.Plot "Plot"
                                    (-> (Const [(Float2 10 3) (Float2 5 6) (Float2 9 10)])
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

                   (GUI.Button "Push me!" (Msg "Action!"))
                   (Cond [(-> (Is true)) (Msg "yeah...")])
                   (GUI.Button "Push me!" (Msg "Action!") GuiButton.Small)
                   (Cond [(-> (Is true)) (Msg "yeah...")])
                   (GUI.Button "Push me!" (Msg "Action!") GuiButton.ArrowUp)
                   (Cond [(-> (Is true)) (Msg "yeah...")])
                   (GUI.ChildWindow :Border true :Height 350 :Contents
                                    (-> (ToBytes)
                                        (GUI.HexViewer)))))
      (GUI.Window "Another window" :Pos (Float2 0.5 0.5) :Width 0.25 :Height 0.25
                  :AllowMove true
                  :Contents
                  (->
                   "Hi" (GUI.Text)))])))

(schedule Root test-chain)
(run Root 0.02 100)
;; (run Root 0.02)

(schedule Root test-chain)
(run Root 0.02 100)

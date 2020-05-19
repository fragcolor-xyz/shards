(def! Root (Node))

(schedule Root (Chain "neural" :Looped
  (BGFX.MainWindow :Title "SDL Window" :Width 450 :Height 720)
  (DispatchOnce
   (Chain
    "init"
    (LoadImage "../../assets/drawing.png")
    (BGFX.Texture2D)
    (Set "image1" :Global true)
    false (Set "checkBoxie")))
  (ImGui.Window :Title "My ImGui" :Width 450 :Height 720 :PosX 0 :PosY 0 :Contents (--> 
    "Hello world"   (ImGui.Text)
    "Hello world 2" (ImGui.Text)
    "Hello world 3" (ImGui.SameLine) (ImGui.Text)
    "Hello world 4" (ImGui.SameLine) (ImGui.Text)
    (ImGui.Separator)

    (ImGui.Indent)
    99 (ImGui.Text "Player1" (Color 255 0 0 255))
    (ImGui.Unindent)
    99 (ImGui.Text "Player1")
    99 (ImGui.Text "Player1")
    (ImGui.Separator)
    (ImGui.CheckBox "CheckBoxie" "checkBoxie") (ImGui.SameLine) (ImGui.Text)
    (Float4 2 3 4 5)
    (Set "x")
    (Float4 1 2 3 4)
    (Math.Add .x)
    (ImGui.Text)

    (Get "image1")
    (ImGui.Image (Float2 0.1 0.1))

    (ImGui.TreeNode
     "Node1"
     (-->
      "Node text..."
      (ImGui.Text)
      (ImGui.TextInput "Say something" "text1")
      (ImGui.Text "<-- you said!")

      (ImGui.IntInput)
      (ImGui.Text)

      (ImGui.FloatInput)
      (ImGui.Text)

      (ImGui.Int3Input)
      (ImGui.Text)

      (ImGui.Float3Input "f3" "f3var")
      (Get "f3var")
      (ImGui.Text)

      (ImGui.FloatDrag)
      (ImGui.Text)

      (ImGui.Float3Drag)
      (ImGui.Text)
      ))
    (ImGui.Button "Push me!" (-->
      (Msg "Action!")
    ))
    (Cond [(--> (Is true)) (--> (Msg "yeah..."))])
    (ImGui.Button "Push me!" (-->
      (Msg "Action!")
    ) ImGuiButton.Small)
    (Cond [(--> (Is true)) (--> (Msg "yeah..."))])
    (ImGui.Button "Push me!" (-->
      (Msg "Action!")
    ) ImGuiButton.ArrowUp)
    (Cond [(--> (Is true)) (--> (Msg "yeah..."))])
    (ToBytes)
    (ImGui.HexViewer)
    ))
  (BGFX.Draw)
))

(run Root 0.02)

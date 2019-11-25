(def! Root (Node))

(schedule Root (Chain "neural" :Looped
  (BGFX.MainWindow :Title "SDL Window" :Width 400 :Height 720)
  (DispatchOnce (Chain "init"
    false (Set "checkBoxie")))
  (ImGui.Window :Title "My ImGui" :Width 400 :Height 720 :PosX 0 :PosY 0 :Contents (--> 
    "Hello world"   (ImGui.Text)
    "Hello world 2" (ImGui.Text)
    "Hello world 3" (ImGui.SameLine) (ImGui.Text)
    "Hello world 4" (ImGui.SameLine) (ImGui.Text)
    (ImGui.Separator)
    ; (ToBytes)
                                        ; (ImGui.HexViewer)
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
    (Math.Add (# "x"))
    (ImGui.Text)

    (ImGui.InputText "Say something" "text1")
    (ImGui.Text "<-- you said!")

    (ImGui.TreeNode
     "Node1"
     (-->
      "Node text..."
      (ImGui.Text)))
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
    (Cond [(--> (Is true)) (--> (Msg "yeah..."))])))
  (BGFX.Draw)
))

(run Root 0.02)

(def! Root (Node))
(schedule Root (Chain "MainLoop" :Looped
  (BGFX.MainWindow :Title "My Window" :Width 400 :Height 200)
  (ImGui.Window "My ImGui Window" :Width 400 :Height 200 
                :PosX 0 :PosY 0 :Contents (--> 
    "Hello world"   (ImGui.Text)
    "Hello world 2" (ImGui.Text) 
    "Hello world 3" (ImGui.Text)
    "Hello world 4" (ImGui.SameLine) (ImGui.Text)
    (ImGui.Button "Push me!" (-->
      (Msg "Action!")))
    (ImGui.CheckBox)
    (Cond [(--> (Is true)) (-->
        "Hello optional world" (ImGui.Text))])))
  (BGFX.Draw)))
(run Root 0.02)
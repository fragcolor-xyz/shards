(do
  (Chain
   "Reloadable"
   
   "Hello world!"
   (ImGui.Text)
   "Next text"
   (ImGui.Text)
   (ImGui.InputText "Say something" "text1")
   (ImGui.Text)
   (ImGui.Separator)
   (ImGui.CheckBox "Yes/No" "cbox1")
   (ImGui.Text)
   
   ))

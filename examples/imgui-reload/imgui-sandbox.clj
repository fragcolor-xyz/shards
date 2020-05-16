(do
  (Chain
   "Reloadable"

   "Hello world!"
   (ImGui.Text)

   "Next text"
   (ImGui.Text)

   (ImGui.CheckBox "Yes/No" "cbox1")
   (ImGui.Text)

   (ImGui.Separator)

   (ImGui.TextInput "Say something" "text1")
   (ImGui.Text)))

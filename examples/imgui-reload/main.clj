(load-file "imgui-style.clj")

(def Root (Node))

(schedule
 Root
 (Chain
  "main" :Looped
  (BGFX.MainWindow :Title "SDL Window" :Width 400 :Height 720)
  (DispatchOnce(Chain
                "init"
                "" (Set "text1")
                (applyStyle)))
  (ImGui.Window
   :Title "Hot-reloaded window"
   :Width 400 :Height 720 :PosX 0 :PosY 0
   :Contents
   (-->
    (ChainLoader (Chain@ "imgui-sandbox.clj"))))
  (BGFX.Draw)))

(run Root 0.0167)

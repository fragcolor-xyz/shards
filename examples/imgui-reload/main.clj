(load-file "imgui-style.clj")

(def Root (Node))

(schedule
 Root
 (Chain
  "main" :Looped
  (GFX.MainWindow
   :Title "SDL Window" :Width 400 :Height 720
   :Contents
   (->
    (Once (->
           "" (Set "text1")
           (applyStyle)))
    (GUI.Window
     :Title "Hot-reloaded window"
     :Width 400 :Height 720 :PosX 0 :PosY 0
     :Contents
     (-> (ChainLoader (Chain* "imgui-sandbox.clj"))))))))

(run Root 0.0167)

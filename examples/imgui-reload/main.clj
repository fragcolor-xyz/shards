; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

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
     :Width 400 :Height 720 :Pos (int2 0, 0)
     :Flags [GuiWindowFlags.NoResize, GuiWindowFlags.NoMove, GuiWindowFlags.NoCollapse]
     :Contents
     (-> (ChainLoader (Chain* "imgui-sandbox.clj"))))))))

(run Root 0.0167)

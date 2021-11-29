; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

(do
  (def action
    (Chain
     "buttonAction"
     (Pause 2.0)
     (Msg "This happened 2 seconds later")))

  (def Root (Node))

  (schedule
   Root
   (Chain
    "MainLoop" :Looped
    (GFX.MainWindow
     :Title "My Window" :Width 400 :Height 200
     :Contents
     (-> (GUI.Window "My ImGui Window"
                     :Width 400 :Height 200 :Pos (Int2 0 0)
                     :Contents
                     (-> "Hello world"   (GUI.Text)
                         "Hello world 2" (GUI.Text)
                         "Hello world 3" (GUI.Text)
                         "Hello world 4" (GUI.SameLine) (GUI.Text)
                         (GUI.Button "Push me!"
                                     (-> (Msg "Action!")
                                         (Await (Log "Hello worker!"))
                                         action >= .chainVar
                                         (ChainRunner .chainVar :Mode RunChainMode.Detached)
                                    ;; (Detach action)
                                         ))
                         (GUI.Checkbox :Variable .checked)
                         .checked
                         (When (Is true)
                               (-> "Hello optional world"
                                   (GUI.Text)))))))))

  (run Root 0.02))
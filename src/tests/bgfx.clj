(def! Root (Node))

(schedule
 Root
 (Chain
  "neural"
  :Looped
  (GFX.MainWindow :Title "SDL Window" :Width 1024 :Height 1024)
  (Once (Dispatch
         (Chain
          "init"
          (LoadImage "../../assets/drawing.png")
          (GFX.Texture2D)
          (Set "image1" :Global true)
          false (Set "checkBoxie"))))
  (GUI.Window :Title "My ImGui" :Width 1024 :Height 1024 :PosX 0 :PosY 0 :Contents
                (-->
                 "Hello world"   (GUI.Text)
                 "Hello world 2" (GUI.Text)
                 "Hello world 3" (GUI.SameLine) (GUI.Text)
                 "Hello world 4" (GUI.SameLine) (GUI.Text)
                 (GUI.Separator)

                 (GUI.Indent)
                 99 (GUI.Text "Player1" (Color 255 0 0 255))
                 (GUI.Unindent)
                 99 (GUI.Text "Player1")
                 99 (GUI.Text "Player1")
                 (GUI.Separator)
                 (GUI.CheckBox "CheckBoxie" "checkBoxie") (GUI.SameLine) (GUI.Text)
                 (Float4 2 3 4 5)
                 (Set "x")
                 (Float4 1 2 3 4)
                 (Math.Add .x)
                 (GUI.Text)

                 (Get "image1")
                 (GUI.Image (Float2 0.1 0.1))

                 (GUI.ChildWindow :Border true :Contents
                                    #((GUI.TreeNode
                                       "Node1"
                                       (-->
                                        "Node text..."
                                        (GUI.Text)
                                        (GUI.TextInput "Say something" "text1")
                                        (GUI.Text "<-- you said!")

                                        (GUI.IntInput)
                                        (GUI.Text)

                                        (GUI.FloatInput)
                                        (GUI.Text)

                                        (GUI.Int3Input)
                                        (GUI.Text)

                                        (GUI.Float3Input "f3" "f3var")
                                        (Get "f3var")
                                        (GUI.Text)

                                        (GUI.FloatDrag)
                                        (GUI.Text)

                                        (GUI.Float3Drag)
                                        (GUI.Text)
                                        (GUI.Plot "Plot"
                                                    #((Const [(Float2 10 3) (Float2 5 6) (Float2 9 10)])
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

                 (GUI.Button "Push me!" (-->
                                           (Msg "Action!")))
                 (Cond [(--> (Is true)) (--> (Msg "yeah..."))])
                 (GUI.Button "Push me!" (-->
                                           (Msg "Action!")) ImGuiButton.Small)
                 (Cond [(--> (Is true)) (--> (Msg "yeah..."))])
                 (GUI.Button "Push me!" (-->
                                           (Msg "Action!")) ImGuiButton.ArrowUp)
                 (Cond [(--> (Is true)) (--> (Msg "yeah..."))])
                 (GUI.ChildWindow :Border true :Width 100 :Height 100 :Contents
                                    #((ToBytes)
                                      (GUI.HexViewer)))))
  (GFX.Draw)))

(run Root 0.02 100)
;; (run Root 0.02)

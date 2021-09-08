; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

(do
  (Chain
   "Reloadable"

   "Hello world!"
   (GUI.Text)

   "Next text"
   (GUI.Text)

   (GUI.CheckBox "Yes/No" "cbox1")
   (GUI.Text)

   (GUI.Separator)

   (GUI.TextInput "Say something" "text1")
   (GUI.Text)))

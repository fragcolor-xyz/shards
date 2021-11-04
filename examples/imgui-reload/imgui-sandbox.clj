; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

"Hello world!"
(GUI.Text)

"Next text"
(GUI.Text)

(Setup false >= .cbox1)
(GUI.Checkbox "Yes/No" .cbox1)
(GUI.Text)

(GUI.Separator)

(GUI.TextInput "Say something" .text1)
(GUI.Text)

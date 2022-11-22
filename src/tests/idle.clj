; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Mesh))

(def detect
  (Wire
   "detect"
   :Looped
   (Desktop.LastInput)
   (Log)))

(schedule Root detect)
(if (run Root 1.0) nil (throw "Root tick failed"))
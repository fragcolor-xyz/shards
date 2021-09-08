; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Node))

(def detect
  (Chain
   "detect"
   :Looped
   (Desktop.LastInput)
   (Log)))

(schedule Root detect)
(run Root 1.0)
; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

;; Sample 1

(schedule
 (Wire
  "wire-1" :Looped
  (Get "x" :Default 0)
  (Log)
  (Inc "x")))

(schedule
 (Wire
  "wire-2" :Looped
  (Get "x" :Default 0)
  (Log)))

                                        ;wire-1> 0
                                        ;wire-2> 1

;; Sample 2

(schedule
 (Wire
  "wire-1" :Looped
  (Get "x" :Default 0)
  (Log)
  (Pause)
  (Inc "x")))

(schedule
 (Wire
  "wire-2" :Looped
  (Get "x" :Default 0)
  (Log)))

                                        ;wire-1> 0
                                        ;wire-2> 0
                                        ;wire-1> 0 ;; wire yields by default at end of iteration!
                                        ;wire-2> 1


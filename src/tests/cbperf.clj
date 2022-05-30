; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

; valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes ./cblp ../../shards/src/tests/cbperf.clj
(def Root (Mesh))
(schedule Root (Wire "analysis" ; :Looped
  18000000
  (Set "nfloats")  
  (Profile (->
    0 (Set "idx")
    (Repeat (->
      ; (Get "idx") ; LR per call 79
      (Get "idx") 
      (Math.Add 1)
      (Update "idx")
    ) .nfloats)
    ))))
(run Root 0.01)

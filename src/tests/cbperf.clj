; valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes ./cblp ../../chainblocks/src/tests/cbperf.clj
(def Root (Node))
(schedule Root (Chain "analysis" ; :Looped
  18000000
  (Set "nfloats")  
  (Profile (-->
    0 (Set "idx")
    (Repeat (-->
      ; (Get "idx") ; LR per call 79
      (Get "idx") 
      (Math.Add 1)
      (Update "idx")
    ) (# "nfloats"))
    ))))
(run Root 0.01)

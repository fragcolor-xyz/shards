(def Root (Node))

(schedule
 Root
 (Chain
  "test"
  (Repeat
   (-->
    (Evolve
     (Chain
      "evolveme"
      (Mutant (Const 10))
      (Log)
      (Msg "evolution happens here...")))
    (Log))
   4)
  ))

(run Root 0.1)

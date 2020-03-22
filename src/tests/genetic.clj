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
      (RandomFloat 100.0)
      (Log "evolution happens here... fitness:")
      ))
    (Log))
   4)
  ))

(run Root 0.1)

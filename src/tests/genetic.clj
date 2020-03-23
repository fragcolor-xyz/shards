(def Root (Node))

(def fitness
  (Chain
   "fitness"
   (Math.Subtract 36)
   (ToFloat)
   (Math.Abs)
   (Math.Multiply -1.0)))

(schedule
 Root
 (Chain
  "test"
  (Repeat
   (-->
    (Evolve
     (Chain
      "evolveme"
      (Mutant (Const 10) [0])
      (Mutant (Math.Multiply 2) [0])
      (Log)
      (Do fitness)
      (Log "evolution happens here... fitness:")
      ))
    (Log))
   5)
  ))

(run Root 0.1)

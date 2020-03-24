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
      (Log "Const")
      (Mutant (Math.Multiply 2) [0] [(-->
                                      (RandomInt 10)
                                      (Math.Add 1))])
      (Log)
      (Do fitness)
      (Log "evolution happens here... fitness")
      ))
    (Log)
    (Ref "best"))
   10)
  (Get "best")
  (Take 1)
  (ToJson)
  (Log)
  ))

(run Root 0.1)

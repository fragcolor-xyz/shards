(def Root (Node))

(def fitness
  (Chain
   "fitness"
   (Math.Subtract 36)
   (ToFloat)
   (Math.Abs)
   (Math.Multiply -1.0)
   (Log "evolution happens here... fitness")))

(def evolveme
  (Chain
   "test"
   (Sequence .best :Types [Type.Float Type.Chain])
   (Repeat
    (-->
     (Evolve
      (Chain
       "evolveme"
       (Mutant (Const 10) [0])
       (Pause)
       (Mutant (Math.Multiply 2) [0] [(-->
                                       (RandomInt 10)
                                       (Math.Add 1))]))
      fitness
      :Population 64
      :Coroutines 8)
     (Log) > .best)
    15)
   .best
   (Take 1)
   (ToJson)
   (Log)
   ))

(schedule Root evolveme)
(run Root 0.1)
(def evolveme nil)
(def Root nil)
(prn "Done")

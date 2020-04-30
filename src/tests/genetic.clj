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
(def Root nil)
(def evolveme nil)
(def fitness nil)
(prn "Done 1")

(def Root (Node))

(def fitness
  (Chain
   "fitness"
   (Math.Subtract 36)
   (ToFloat)
   (Math.Abs)
   (Math.Multiply -1.0)
   (Log "evolution happens here... fitness")))

(def state1
  (Chain
   "s1"
   (Msg "into state 1")
   (Start "s2")
   (Msg "back to state 1")
   (Resume "stepped")))

(def state2
  (Chain
   "s2"
   (Msg "into state 2")
   (Resume "s1")))

(def evolveme
  (Chain
   "test"
   (Sequence .best :Types [Type.Float Type.Chain])
   (Repeat
    (-->
     (Evolve
      (Chain
       "evolveme"
       :Looped
       (Once (--> 0 >= .niters))
       .niters (Math.Inc) > .niters
       (When (IsMore 10) (Stop))
       (Step (Chain "stepped" (Start state1)))
       (Mutant (Const 10) [0])
       (Mutant (Math.Multiply 2) [0] [(-->
                                       (RandomInt 10)
                                       (Math.Add 1))]))
      fitness
      :Population 100
      :Threads 10
      :Coroutines 1)
     (Log) > .best)
    1)
   .best
   (Log)
   ))

(schedule Root evolveme)
(run Root 0.1)
(def Root nil)
(def evolveme nil)
(def fitness nil)
(prn "Done 2")

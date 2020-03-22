(def Root (Node))

(schedule
 Root
 (Chain
  "test"
  :Looped
  (Evolve
   (Chain
    "evolveme"
    (Mutant (Const 10))
    (Log)
    (Msg "evolution happens here...")))
  (Log)))

(run Root 0.1 2)

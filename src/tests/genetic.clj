(def Root (Node))

(schedule Root (Chain
                "test"
                (Evolve (Chain
                         "evolveme"
                         (Msg "evolution happens here...")))
                (Log)))

(run Root 0.1 10)

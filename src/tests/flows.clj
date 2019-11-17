(def Root (Node))

(def chain1
       (Chain
        "one"
        (Msg "one - 1")
        (ContinueChain "two")
        (Msg "one - 2")
        (Msg "one - 3")
        (ContinueChain "two")
        (Msg "one - Done")
        (ContinueChain "two")
        ))

(def chain2
       (Chain
        "two"
        (Msg "two - 1")
        (ContinueChain "one")
        (Msg "two - 2")
        (Msg "two - 3")
        (ContinueChain "one")
        (Msg "two - 4")
        (Msg "two - Done")
        ))

(prepare chain2)
(schedule Root chain1)
(run Root 0.1)

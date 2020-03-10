(def Root (Node))
(schedule Root (Chain
                "py"
                10
                (Py "PyBlock1")
                (Assert.Is 20 true)
                (Math.Add 20)
                (Assert.Is 40 true)))
(run Root 0.1)

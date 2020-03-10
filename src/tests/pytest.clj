(def Root (Node))
(schedule Root (Chain
                "py1"
                10
                (Py "PyBlock1")
                (Assert.Is 20 true)
                (Math.Add 20)
                (Assert.Is 40 true)
                (Log)))

(schedule Root (Chain
                "py2"
                20
                (Py "PyBlock1")
                (Assert.Is 30 true)
                (Math.Add 20)
                (Assert.Is 50 true)
                (Log)))
(run Root 0.1)

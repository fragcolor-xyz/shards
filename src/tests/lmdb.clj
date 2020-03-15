(def Root (Node))

(schedule Root (Chain
                "test1"
                :Looped
                "Hello DataBase world!"
                (DB.Update "MyKey")
                (DB.Get "MyKey")
                (Assert.Is "Hello DataBase world!" true)
                (Log)
                (Const [1 2 3 4])
                (DB.Update "MySeq")
                (DB.Get "MySeq")
                (Assert.Is [1 2 3 4] true)
                (Log)
                ))

(run Root 0.1 10)

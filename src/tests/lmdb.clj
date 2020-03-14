(def Root (Node))

(schedule Root (Chain
                "test1"
                "Hello DataBase world!"
                (DB.Update "MyKey")
                (DB.Get "MyKey")
                (Assert.Is "Hello DataBase world!" true)
                (Log)))

(run Root 0.1 10)

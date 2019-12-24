(def Root (Node))
(def func1
  (Chain
   "f1"
   10
   (Push)
   (Log)
                                        ; don't pop, we test integrity
   ))

(schedule Root
          (Chain
           "main"
           11
           (Push)
           (Do func1)
           (Log)
           (Assert.Is 10 true)
           (Pop)
           (Log)
           (Assert.Is 11 true)
           ))

(def Root (Node))
(def func1
  (Chain
   "f1"
   10
   (Push)
   (GetStack)
   (Assert.Is [11 10] true)
   (Take 1)
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

           5
           (Push)
           6
           (Push)
           (GetStack)
           (Log)
           (Assert.Is [5 6] true)
           (Swap)
           (Pop)
           (Assert.Is 5 true)
           (Pop)
           (Assert.Is 6 true)
           ))

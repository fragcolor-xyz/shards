(def Root (Node))

; fail during await, uses awaitne(...)
(def c
  (Chain
   "test-1"
   (Await ~[false (Assert.Is true)])))

(schedule Root c)
(run Root 0.1)

; fail during http resolver, uses await(...)
(def c
  (Chain
   "test-2"
   (Http.Get "abc.abcdefg")))

(schedule Root c)
(run Root 0.1)


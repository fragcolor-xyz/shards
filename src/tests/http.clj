(def Root (Node))

(def test
  (Chain
   "test"
   (Http.Get)
   ))

(schedule Root test)

(run Root 0.1 10)

(def Root (Node))
(schedule Root (Chain "tests"
  (Float3 1 2 3)
  (Math.LinAlg.Cross (Float3 2 2 2))
  (Log)
  (Assert.Is (Float3 -2 4 -2) true)

  (Const [(Float3 1 2 3), (Float3 1 3 4)])
  (Math.LinAlg.Cross (Float3 2 2 2))
  (Log)
  (Assert.Is [(Float3 -2 4 -2) (Float3 -2 6 -4)] true)

  (Const [(Float3 1 2 3), (Float3 1 3 4)])
  (Math.LinAlg.Cross [(Float3 2 2 2) (Float3 3 3 3)])
  (Log)
  (Assert.Is [(Float3 -2 4 -2) (Float3 -3 9 -6)] true)
))

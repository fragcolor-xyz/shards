(def identity [(Float4 1 0 0 0)
               (Float4 0 1 0 0)
               (Float4 0 0 1 0)
               (Float4 0 0 0 1)])

(def mat1 [(Float4 1 2 3 4)
           (Float4 1 2 3 4)
           (Float4 1 2 3 4)
           (Float4 1 2 3 4)])

(def mat1t [(Float4 1 1 1 1)
            (Float4 2 2 2 2)
            (Float4 3 3 3 3)
            (Float4 4 4 4 4)])

(def mat2 [(Float2 1 2)
           (Float2 1 2)
           (Float2 1 2)
           (Float2 1 2)])

(def mat2t [(Float4 1 1 1 1)
            (Float4 2 2 2 2)])

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

  (Float3 1 2 3)
  (Math.LinAlg.Dot (Float3 1 5 7))
  (Log)
  (Assert.Is (Float 32) true)

  (Float3 1 2 3)
  (Math.LinAlg.LengthSquared)
  (Log)
  (Assert.Is 14.0 true)

  (Float3 1 1 2)
  (Math.LinAlg.Length)
  (Log)
  (ToString) ; truncate hack
  (Assert.Is "2.44949" true)

  (Float3 1 0 1)
  (Math.LinAlg.Normalize)
  (Log)

  (Float3 4270.56 4602.19 188) (Ref "playerPos")
  ; (Float3 4277.35 4596.54 188) (Set "targetPos")
  (Float3 4283.18 4604.73 188) (Ref "targetPos")
  (Get "targetPos") (Math.Subtract .playerPos)
  (Math.LinAlg.Normalize)
  (Log)

  (Float4 2 3 4 5)
  (Ref "vec4.1")
  
  (Const identity)
  (Math.LinAlg.MatMul (Float4 1 2 3 4))
  (Log)
  (Assert.Is (Float4 1 2 3 4) true)

  (Const identity)
  (Math.LinAlg.MatMul .vec4.1)
  (Log)
  (Assert.Is (Float4 2 3 4 5) true)
  (Ref "vec4.2")

  (Const identity)
  (Math.LinAlg.MatMul .vec4.2)
  (Log)
  (Assert.Is (Float4 2 3 4 5) true)

  (Const identity)
  (Math.LinAlg.MatMul identity)
  (Log)
  (Assert.Is identity true)

  (Const mat1)
  (Math.LinAlg.Transpose)
  (Log)
  (Assert.Is mat1t true)

  (Const mat2)
  (Math.LinAlg.Transpose)
  (Log)
  (Assert.Is mat2t true)

  (Math.LinAlg.Orthographic 2560 1440 0 10000)
  (Math.LinAlg.Transpose)
  (Log)

  (Msg "Done!")
))

(tick Root)

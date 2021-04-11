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
  (Math.Cross (Float3 2 2 2))
  (Log)
  (Assert.Is (Float3 -2 4 -2) true)

  (Const [(Float3 1 2 3), (Float3 1 3 4)])
  (Math.Cross (Float3 2 2 2))
  (Log)
  (Assert.Is [(Float3 -2 4 -2) (Float3 -2 6 -4)] true)

  (Const [(Float3 1 2 3), (Float3 1 3 4)])
  (Math.Cross [(Float3 2 2 2) (Float3 3 3 3)])
  (Log)
  (Assert.Is [(Float3 -2 4 -2) (Float3 -3 9 -6)] true)

  (Float3 1 2 3)
  (Math.Dot (Float3 1 5 7))
  (Log)
  (Assert.Is (Float 32) true)

  (Float3 1 2 3)
  (Math.LengthSquared)
  (Log)
  (Assert.Is 14.0 true)

  (Float3 1 1 2)
  (Math.Length)
  (Log)
  (ToString) ; truncate hack
  (Assert.Is "2.44949" true)

  (Float3 1 0 1)
  (Math.Normalize)
  (Log)

  (Float3 4270.56 4602.19 188) (Ref "playerPos")
  ; (Float3 4277.35 4596.54 188) (Set "targetPos")
  (Float3 4283.18 4604.73 188) (Ref "targetPos")
  (Get "targetPos") (Math.Subtract .playerPos)
  (Math.Normalize)
  (Log)

  (Float4 2 3 4 5)
  (Ref "vec4.1")
  
  (Const identity)
  (Math.MatMul (Float4 1 2 3 4))
  (Log)
  (Assert.Is (Float4 1 2 3 4) true)

  (Const identity)
  (Math.MatMul .vec4.1)
  (Log)
  (Assert.Is (Float4 2 3 4 5) true)
  (Ref "vec4.2")

  (Const identity)
  (Math.MatMul .vec4.2)
  (Log)
  (Assert.Is (Float4 2 3 4 5) true)

  (Const identity)
  (Math.MatMul identity)
  (Log)
  (Assert.Is identity true)

  (Const mat1)
  (Math.Transpose)
  (Log)
  (Assert.Is mat1t true)

  (Const mat2)
  (Math.Transpose)
  (Log)
  (Assert.Is mat2t true)

  (Math.Orthographic 2560 1440 0 10000)
  (Math.Transpose)
  (Log)

  (Float3 2 3 4) (Math.Scaling)
  (Assert.Is [(Float4 2 0 0 0) (Float4 0 3 0 0) (Float4 0 0 4 0) (Float4 0 0 0 1)] true)
  (Log)

  (Float3 2 3 4) (Math.Translation)
  (Assert.Is [(Float4 1 0 0 0) (Float4 0 1 0 0) (Float4 0 0 1 0) (Float4 2 3 4 1)] true)
  (Log)

  180.0 (Math.DegreesToRadians) (Log) (Sub (-> (ToString) (Assert.Is "3.14159" true)))
  (Math.AxisAngleX) (Log)
  (Math.Rotation) (Log) 

  (/ 3.141592653589793238463 2.0)
  (Math.AxisAngleY) (Log)
  (Math.Rotation) (Log) 
  ; TODO float precision-less asserts
  ;; (Sub (-> (ToString) (Assert.Is "[(0, 0, -1, 0), (0, 1, 0, 0), (1, 0, 0, 0), (0, 0, 0, 1)]" true)))
  
  (Float3 -1.0 1.0 2.0) (Math.Normalize :Positive false) (Log)
  [-1.0 1.0 2.0] (Math.Normalize :Positive false) (Log)
  (Float3 -1.0 1.0 2.0) (Math.Normalize :Positive true) (Log)
  [-1.0 1.0 2.0] (Math.Normalize :Positive true) (Log)

  (Float2 -1.0 1.0) (Math.Normalize :Positive false) (Log)
  [-1.0 1.0] (Math.Normalize :Positive false) (Log)
  (Float2 -1.0 1.0) (Math.Normalize :Positive true) (Log)
  [-1.0 1.0] (Math.Normalize :Positive true) (Log)

  (Float2 20.0 30.0) (Math.Normalize :Positive false) (Log)
  [20.0 30.0] (Math.Normalize :Positive false) (Log)
  (Float2 20.0 30.0) (Math.Normalize :Positive true) (Log)
  [20.0 30.0] (Math.Normalize :Positive true) (Log)

  (Msg "Done!")
))

(tick Root)

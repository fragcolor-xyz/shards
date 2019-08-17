(def! Root (Node))

(def! inner1 (Chain "inner"
  (Assert.Is "My input" true)
  "My input 2"
))

(def! testChain (Chain "namedChain"
  (Msg "Running tests!")
  
  true
  (Cond [
    (--> (Is true)) (--> (Msg "Cond was true!!") false)
    (--> (Is false)) (--> (Msg "Cond was false!") true)])
  (Assert.Is false true)

  true
  (Cond [
    (--> (Is false) (Or) (Is true)) (--> (Msg "Cond was true!!") false)
    (--> (Is false)) (--> (Msg "Cond was false!") true)])
  (Assert.Is false true)

  true
  (Cond [
    (--> (Is false) (And) (Is true)) (--> (Assert.Is nil true))
    (--> (Is true)) (--> (Msg "Cond was false!") true)])
  (Assert.Is true true)

  false
  (Cond [
    (--> (Is true)) (--> (Msg "Cond was true!!") false)
    (--> (Is false)) (--> (Msg "Cond was false!") true)])
  (Assert.Is true true)

  false
  (Cond [
    (--> (Is true)) (--> (Msg "Cond was true!!") false)
    (--> (Is false) (And) (IsNot true)) (--> (Msg "Cond was false!") true)])
  (Assert.Is true true)

  false
  (Cond [
    (--> (Is false) (And) (IsNot false)) (--> (Assert.Is nil true))
    (--> (Is false)) (--> (Msg "Cond was true!!") false)])
  (Assert.Is false true)

  10
  (Cond [
    (--> true)    (--> (Msg "Cond was true!!") false)
    (--> (Is 10)) (--> (Msg "Cond was false!") true)
  ] :Threading true)
  (Assert.Is true true)

  10
  (Cond [
    (--> true)    (--> (Msg "Cond was true!!") false)
    (--> (Is 10)) (--> (Msg "Cond was false!") true)
  ] :Threading false)
  (Assert.Is false true)

  "Hello"
  (Assert.Is "Hello" true)
  (Log)

  77
  (Assert.Is 77 true)
  (Log)

  10
  (Math.Add 10)
  (Assert.Is (+ 10 10) true)
  (Log)

  11
  (Math.Subtract 10)
  (Assert.Is (- 11 10) true)
  (Log)

  (Float4 10.3 3.6 2.1 1.1)
  (Math.Multiply (Float4 2 2 2 2))
  (Assert.Is (Float4 (* 10.3 2) (* 3.6 2) (* 2.1 2) (* 1.1 2)) true)
  (Log)

  (Float3 10.3 2.1 1.1)
  (Math.Multiply (Float3 2 2 2))
  (Assert.Is (Float3 (* 10.3 2) (* 2.1 2) (* 1.1 2)) true)
  (Log)

  (Const [])
  (Set "list1")
  10
  (Push "list1")
  20
  (Push "list1")
  30
  (Push "list1")
  (Get "list1")
  (Log)
  (Take 0)
  (Assert.Is 10 true)
  (Get "list1")
  (Take 1)
  (Assert.Is 20 true)
  (Get "list1")
  (Take 2)
  (Assert.Is 30 true)
  (Log)

  (Repeat (-->
    (Const [])
    (Set "list1")
    10
    (Push "list1")
    20
    (Push "list1")
    30
    (Push "list1")
    (Get "list1")
    (Log)
    (Take 0)
    (Assert.Is 10 true)
    (Get "list1")
    (Take 1)
    (Assert.Is 20 true)
    (Get "list1")
    (Take 2)
    (Assert.Is 30 true)
    (Log)
  ) :Times 5)

  0
  (Set "counter")
  (Repeat (-->
    (Get "counter")
    (Math.Add 1)
    (Set "counter")
   ) :Times 5)
  (Get "counter")
  (Assert.Is 5 true)
  (Log)

  20
  (Set "a")
  30
  (Set "b")
  (Swap "a" "b")
  (Get "a")
  (Assert.Is 30)
  (Get "b")
  (Assert.Is 20)
  (Log)

  "Value1"
  (Set "tab1" "v1")
  "Value2"
  (Set "tab1" "v2")
  (Get "tab1" "v1")
  (Assert.Is "Value1" true)
  (Log)
  (Get "tab1" "v2")
  (Assert.IsNot "Value1" true)
  (Log)

  ; "chain:initChain[1]"
  ; (ReplaceText "[^abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\\-\\.\\_]+" "_")
  ; (Log)

  "My input"
  (Dispatch inner1)
  (Assert.Is "My input" true)

  "My input"
  (Do inner1)
  (Assert.Is "My input 2" true)

  ; b0r
  0
  (Math.And 0xFF)
  (Math.LShift 8)
  (Set "x")
  ; b1r
  0
  (Math.And 0xFF)
  (Math.Or (# "x"))
  (Math.LShift 8)
  (Set "x")
  ; b2r
  59
  (Math.And 0xFF)
  (Math.Or (# "x"))
  (Math.LShift 8)
  (Set "x")
  ; b3r
  156
  (Math.And 0xFF)
  (Math.Or (# "x"))
  (Set "x")
  ; result
  (Get "x")
  (Log)

  (Color 2 2 2 255)
  (Log)

  (Const [(Float 1) (Float 2) (Float 3) (Float 4)])
  (Math.Multiply (Float 2.0))
  (Log)
  (Assert.Is [(Float 2) (Float 4) (Float 6) (Float 8)] true)

  (Const [(Float 1) (Float 2) (Float 3) (Float 4)])
  (Math.Multiply [(Float 2.0) (Float 1.0) (Float 1.0) (Float 2.0)])
  (Log)
  (Assert.Is [(Float 2) (Float 2) (Float 3) (Float 8)] true)

  5 (ToFloat) (Math.Divide (Float 10))
  (Assert.Is 0.5 true)

  (Int 10) (ToFloat) (Set "fx")
  (Get "fx") (Assert.Is (Float 10) true)
  (Get "fx") (Assert.IsNot (Int 10) true)
  (Float 5) (Math.Divide (# "fx")) (Assert.Is 0.5 true)
  
  ; 5 (ToFloat) (Math.Divide (Float 0))
  ; (Assert.Is 0.5 true)

  (Msg "All looking good!")
))
(schedule Root testChain)
(if (tick Root) nil (throw "Root tick failed"))

; test json support
(schedule Root (ChainJson (json testChain)))
(if (tick Root) nil (throw "Root tick failed"))
; (println (json testChain))

; (def! P (Node))
; (def! C (Node))
; (schedule P (Chain "producer" :Looped 
;   "Hello world!"
;   (IPC.Push "shared1")
; ))

; (schedule C (Chain "consumer" :Looped 
;   (IPC.Pop "shared1")
;   (Assert.Is "Hello world!" true)
;   (Log)
; ))
; (if (tick P) nil (throw "P/C tick failed"))
; (if (tick C) nil (throw "P/C tick failed"))
; (if (tick P) nil (throw "P/C tick failed"))
; (if (tick C) nil (throw "P/C tick failed"))
; (if (tick P) nil (throw "P/C tick failed"))
; (if (tick C) nil (throw "P/C tick failed"))

; (def! inc (fn* [a] (+ a 1)))

; (def! Loop (fn* [counter] (do
;   (tick Root)
;   (sleep 0.1)
;   (prn counter)
;   (Loop (inc counter))
; )))

; (Loop 0)
(def! Root (Node))

(def! inner1 (Chain "inner"
  (Assert.Is "My input" true)
  "My input 2"
))

(def! testChain (Chain "namedChain"
  (Msg "Running tests!")

  true
  (Cond [
    (--> (When true) true) (--> (Msg "Cond was true!!") false)
    (--> (When false) true) (--> (Msg "Cond was false!") true)])
  (Assert.Is false true)

  false
  (Cond [
    (--> (When true) true) (--> (Msg "Cond was true!!") false)
    (--> (When false) true) (--> (Msg "Cond was false!") true)])
  (Assert.Is true true)

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

  nil
  (AddVariable "list1") ; should reset the list..
  10
  (AddVariable "list1")
  20
  (AddVariable "list1")
  30
  (AddVariable "list1")
  (GetVariable "list1")
  (Log)
  (GetItems 0)
  (Assert.Is 10 true)
  (GetVariable "list1")
  (GetItems 1)
  (Assert.Is 20 true)
  (GetVariable "list1")
  (GetItems 2)
  (Assert.Is 30 true)
  (Log)

  (Repeat (-->
    nil
    (AddVariable "list1") ; should reset the list..
    10
    (AddVariable "list1")
    20
    (AddVariable "list1")
    30
    (AddVariable "list1")
    (GetVariable "list1")
    (Log)
    (GetItems 0)
    (Assert.Is 10 true)
    (GetVariable "list1")
    (GetItems 1)
    (Assert.Is 20 true)
    (GetVariable "list1")
    (GetItems 2)
    (Assert.Is 30 true)
    (Log)
  ) :Times 5)

  0
  (SetVariable "counter")
  (Repeat (-->
    (GetVariable "counter")
    (Math.Add 1)
    (SetVariable "counter")
   ) :Times 5)
  (GetVariable "counter")
  (Assert.Is 5 true)
  (Log)

  20
  (SetVariable "a")
  30
  (SetVariable "b")
  (SwapVariables "a" "b")
  (GetVariable "a")
  (Assert.Is 30)
  (GetVariable "b")
  (Assert.Is 20)
  (Log)

  "Value1"
  (SetTableValue "tab1" "v1")
  "Value2"
  (SetTableValue "tab1" "v2")
  (GetTableValue "tab1" "v1")
  (Assert.Is "Value1" true)
  (Log)
  (GetTableValue "tab1" "v2")
  (Assert.IsNot "Value1" true)
  (Log)

  "chain:initChain[1]"
  (ReplaceText "[^abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\\-\\.\\_]+" "_")
  (Log)

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
  (SetVariable "x")
  ; b1r
  0
  (Math.And 0xFF)
  (Math.Or (# "x"))
  (Math.LShift 8)
  (SetVariable "x")
  ; b2r
  59
  (Math.And 0xFF)
  (Math.Or (# "x"))
  (Math.LShift 8)
  (SetVariable "x")
  ; b3r
  156
  (Math.And 0xFF)
  (Math.Or (# "x"))
  (SetVariable "x")
  ; result
  (GetVariable "x")
  (Log)

  (Msg "All looking good!")
))
(schedule Root testChain)
(if (tick Root) nil (throw "Root tick failed"))

; test json support
(schedule Root (ChainJson (json testChain)))
(if (tick Root) nil (throw "Root tick failed"))
(println (json testChain))

(def! P (Node))
(def! C (Node))
(schedule P (Chain "producer" :Looped 
  "Hello world!"
  (IPC.Push "shared1")
))

(schedule C (Chain "consumer" :Looped 
  (IPC.Pop "shared1")
  (Assert.Is "Hello world!" true)
  (Log)
))
(if (tick P) nil (throw "P/C tick failed"))
(if (tick C) nil (throw "P/C tick failed"))
(if (tick P) nil (throw "P/C tick failed"))
(if (tick C) nil (throw "P/C tick failed"))
(if (tick P) nil (throw "P/C tick failed"))
(if (tick C) nil (throw "P/C tick failed"))

; (def! inc (fn* [a] (+ a 1)))

; (def! Loop (fn* [counter] (do
;   (tick Root)
;   (sleep 0.1)
;   (prn counter)
;   (Loop (inc counter))
; )))

; (Loop 0)
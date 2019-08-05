(def! Root (Node))

; (schedule Root (Chain "namedChain"
;   nil
;   "Hello world!"
;   (When :Accept "\\w+ world!" :IsRegex true)
;   (Msg "Here we are!")
;   (Repeat :Times 5 :Action (Blocks 
;     (Msg "Ok...")
;     (Msg "Ok2?")
;     10
;     (Math.Multiply 10)
;     (Assert.Is 100)
;     (Log)
;   ))
; ))

(schedule Root (Chain "namedChain"
  ; (Msg "Running tests!")

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

  (. 
    (fn* [inputType] inputType) 
    (fn* [input] input)) ; test our interop special block
  (Assert.Is (Float3 (* 10.3 2) (* 2.1 2) (* 1.1 2)) true)
  (Log)

  (. 
    (fn* [inputType] :Int) 
    (fn* [input] (Int 22))) ; test our interop special block
  (Assert.Is 22 true)
  (Log)

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

  0
  (SetVariable "counter")
  (Repeat (Blocks
    (GetVariable "counter")
    (Math.Add 1)
    (SetVariable "counter")
  ) 5)
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

  (Msg "All looking good!")
))

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
(def! P (Node))
(def! C (Node))
(schedule P (Chain "producer" :Looped
  (. 
    (fn* [inputType] :String) 
    (fn* [input] (String "IPC STRING Hello world...")))
  (Assert.Is "IPC STRING Hello world..." true)
  (IPC.Push "shared1")
  (Log)
))

(schedule C (Chain "consumer" :Looped 
  (IPC.Pop "shared1")
  (. 
    (fn* [inputType] inputType) 
    (fn* [input] input))
  (Assert.Is "IPC STRING Hello world..." true)
  (Log)
  10
  (AddVariable "list1")
  20
  (AddVariable "list1")
  30
  (AddVariable "list1")
  (GetVariable "list1")
  (Log)
))

(def! dec (fn* [a] (- a 1)))
(def! Loop (fn* [count] (do
  (if (tick P) nil (throw "P/C tick failed"))
  (if (tick C) nil (throw "P/C tick failed"))
  (if (> count 0) (Loop (dec count)) nil)
)))

(Loop 30)

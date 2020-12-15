(def! n (Node))
(schedule
 n
 (Chain
  "n"
  "" (Process.Run "echo" ["Hello world"])
  (Log)
  (Assert.Is "Hello world" true)))

(def! dec (fn* [a] (- a 1)))
(def! Loop (fn* [count] (do
  (if (tick n) nil (throw "tick failed"))
  (sleep 0.5)
  (if (> count 0) (Loop (dec count)) nil)
)))

(Loop 5)
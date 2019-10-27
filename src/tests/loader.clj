; TODO, we removed json
(def! n (Node))
(schedule n (Chain "n" :Looped
  (ChainLoader "../src/tests/loadme.clj")
))

(def! dec (fn* [a] (- a 1)))
(def! Loop (fn* [count] (do
  (if (tick n) nil (throw "tick failed"))
  (sleep 0.1)
  (if (> count 0) (Loop (dec count)) nil)
)))

(Loop 30)

(prn "done...")

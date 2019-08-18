(def root (Node))

(defn mychain [] (Chain "mychain"
  "Hello chain"
  (Set "var1")
  (Cond [
    (--> true) (--> 1 (Set "var2"))
    (--> false) (--> 1 (Set "var2"))
  ])))

(schedule root (Chain "root"
  (DispatchOnce (mychain))
  (Get "var1")
  (Log)
  (Get "var2")
  (Log)))
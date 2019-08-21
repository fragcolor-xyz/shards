(def root (Node))

(defn mychain [] (Chain "mychain"
  "Hello chain"
  (Set "var1")
  (Cond [
    (--> true) (--> 1 (Set "var2"))
    (--> false) (--> 1 (Set "var2"))
  ])))

(def otherChain (Chain "other"
  99))

(schedule root otherChain)

(schedule root (Chain "root"
  (DispatchOnce (mychain))
  (Get "var1")
  (Log)
  (Get "var2")
  (Log)
  (WaitChain otherChain)
  (Log)
  (Assert.Is 99 true)))
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

(def funcChain (Chain "func"
  (Set "in")

  (Is 10)
  (Or)
  (Is 20)
  
  (Get "in")
  (Log)
))

(schedule root otherChain)

(schedule root (Chain "root"
  (DispatchOnce (mychain))
  (Get "var1")
  (Log)
  (Get "var2")
  (Log)
  (WaitChain otherChain)
  (Log)
  (Assert.Is 99 true)
  10
  (Do funcChain)
  12
  (Do funcChain)
  20
  (Do funcChain)
  (Msg "done")
  ))
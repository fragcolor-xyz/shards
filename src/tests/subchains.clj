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
  (Is 10)
  (Or)
  (Is 20)
  (Or)
  (Log)
))

(def tickedChain (Chain "ticked"
  (Msg "message 1")
  1
  (Sleep (Float 0))
  
  (Msg "message 2")
  2
  (Sleep (Float 0))
  
  (Msg "message 3")
  3
  (Sleep (Float 0))
  
  (Msg "message 4")
  4
  (Sleep (Float 0))
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
  (Dispatch funcChain)
  12
  (Dispatch funcChain)
  20
  (Dispatch funcChain)

  (RunChain tickedChain :Mode RunChainMode.Stepped)
  (Assert.Is 1 true)
  (Msg "next step")
  (RunChain tickedChain :Mode RunChainMode.Stepped)
  (Assert.Is 2 true)
  (Msg "next step")
  (RunChain tickedChain :Mode RunChainMode.Stepped)
  (Assert.Is 3 true)
  (Msg "next step")
  (RunChain tickedChain :Mode RunChainMode.Stepped)
  (Assert.Is 4 true)
  (Msg "next step")
  (RunChain tickedChain :Mode RunChainMode.Stepped)
  (Assert.Is 4 true)
  (Msg "next step")
  (RunChain tickedChain :Mode RunChainMode.Stepped)
  (Assert.Is 1 true)
  (Msg "next step")

  (Msg "done")
))
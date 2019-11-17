(def root (Node))

(defn mychain [] (Chain "mychain"
  "Hello chain"
  (Set "var1")
  (Cond [
    (--> true) (--> 1 (Set "var2"))
    (--> false) (--> 1 (Set "var2"))
  ])
  (Input) ; test Input
  (Log)
  (Assert.Is "Initial input" true)))

(def otherChain (Chain "other"
  99))

(def funcChain (Chain "func"
  (Is 10)
  (Or)
  (Is 20)
  (Or)
  (Log)
  ))

(def jumpChain
  (Chain
   "jumphere!"
   (Msg "jumped...")
   (ContinueChain "ticked")))

(def tickedChain (Chain "ticked"
  (Msg "message 1")
  1
  (Sleep (Float 0))
  
  (Msg "message 2")
  (ContinueChain "jumphere!")
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
  "Initial input"
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

  (Step tickedChain)
  (Assert.Is 1 true)
  (Msg "next step")
  (Step tickedChain)
  (Assert.Is 1 true) ; jumped
  (Step tickedChain)
  (Assert.Is 2 true)
  (Msg "next step")
  (Step tickedChain)
  (Assert.Is 3 true)
  (Msg "next step")
  (Step tickedChain)
  (Assert.Is 4 true)
  (Msg "next step")
  (Step tickedChain)
  (Assert.Is 4 true)
  (Msg "next step")
  (Step tickedChain)
  (Assert.Is 1 true)
  (Msg "next step")

  (Msg "done")
))

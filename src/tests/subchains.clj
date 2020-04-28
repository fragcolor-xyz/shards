(def root (Node))

(defn mychain [] (Chain "mychain"
  "Hello chain"
  (Set "var1" :Global true)
  (Cond [
    (--> true) (--> 1 (Set "var2" :Global true))
    (--> false) (--> 2 (Set "var2" :Global true))
  ])
  (Input) ; test Input
  (Log)
  (Assert.Is "Initial input" true)))

(def otherChain (Chain "other" 99))

(def funcChain
  (Chain
   "func"
   (If (--> (Is 10) (Or) (Is 20)) (Log))))

(def jumpChain
  (Chain
   "jumphere!"
   (Msg "jumped...")
   (Resume "ticked")))

(def jumpChain2
  (Chain
   "jumphere2!"
   (Msg "jumped...")
   (Resume "ticked2")))

(def stopChain
  (Chain
   "stopping"
   (Msg "resumed stopping")
   (Stop)))

(def tickedChain
  (Chain
   "ticked"
   (Msg "message 1")
   1
   (Pause)
   (Msg "message 2")
                                        ; test jumping to another coro
   (Resume "jumphere!")
   (Msg "ticked resumed")
   2
   (Pause)
   (Msg "message 3")
   3
   (Pause)

   (Msg "message 4")
   4
   (Pause)
                               ; make sure main is not stopped in this case
   (Resume "stopping")
   ;; the flow stopped before!
   ;; this should be unreachable
   false (Assert.Is true true)
   ))

(def tickedChain2
  (Chain
   "ticked2"
   (Msg "message 1")
   1
   (Pause)
   (Msg "message 2")
                                        ; test jumping to another coro
   (Start "jumphere2!")
   (Msg "ticked resumed")
   2
   (Pause)
   (Msg "message 3")
   3
   (Pause)

   (Msg "message 4")
   4
   (Msg "ticked2 done")
   ))

(def startButNotResumed
  (Chain
   "started"
   (Msg "From top!")
   (Resume "root")
   ;; Should not enter here!
   false ; fail on purpose here
   (Assert.Is true true)))

(schedule root otherChain)

(schedule root (Chain "root"
  "Initial input"
  (DispatchOnce (mychain))
  (Get "var1")
  (Log "var1")
  (Get "var2")
  (Log "var2")
  (WaitChain otherChain)
  (Log "otherChain")
  (Assert.Is 99 true)
  10
  (Dispatch funcChain)
  12
  (Dispatch funcChain)
  20
  (Dispatch funcChain)

  ; test a stepped chain that (Stop)s
  (Step tickedChain)
  (Msg "had message 1")
  (Assert.Is 1 true) ; pause after 1

  (Step tickedChain)
  (Msg "had message 2")
  (Assert.Is 1 true) ; resume pauses jumped

  (Step tickedChain)
  (Msg "before ticked resume")
  (Assert.Is 1 true) ; resumed ticked and resumed ticked again so paused

  (Step tickedChain) ; pause after 2
  (Assert.Is 2 true)

  (Step tickedChain) ; resume pauses when going stopping
  (Msg "had message 3")
  (Assert.Is 3 true) ; pause after 3

  (Step tickedChain)
  (Msg "had message 4")
  (Assert.Is 4 true) ; pause after 4

  (Step tickedChain) ; will stop the chain
  (Assert.Is 4 true)
  (Step tickedChain) ; last result chain is done
  (Assert.Is 4 true)
  (Step tickedChain) ; last result chain is done
  (Assert.Is 4 true)

                                        ; test a stepped chain that never stops and rotates
  (Repeat
   (-->
    (Msg "repeating!")
    (Step tickedChain2)
    (Msg "had message 1")
    (Assert.Is 1 true) ; pause after 1

    (Step tickedChain2)
    (Msg "had message 2")
    (Assert.Is 1 true) ; resume pauses jumped

    (Step tickedChain2)
    (Msg "before ticked resume")
    (Assert.Is 1 true) ; resumed ticked and resumed ticked again so paused

    (Step tickedChain2) ; pause after 2
    (Assert.Is 2 true)

    (Step tickedChain2) ; resume pauses when going stopping
    (Msg "had message 3")
    (Assert.Is 3 true) ; pause after 3

    (Step tickedChain2)
    (Msg "had message 4")
    (Assert.Is 4 true) ; pause after 4
    ) :Times 3)

  (Start startButNotResumed)
  (Msg "root resumed")
  (Start startButNotResumed)
  (Msg "root resumed")

  (Msg "done")
))

(run root 0.1)

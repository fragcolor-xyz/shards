; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

(def root (Mesh))

(def mywire
  (Wire
   "mywire"
   "Hello wire"
   (Set "var1" :Global true)
   (Cond [
          (-> true) (-> 1 (Set "var2" :Global true))
          (-> false) (-> 2 (Set "var2" :Global true))
          ])
   (Input) ; test Input
   (Log)
   (Assert.Is "Initial input" true)))

(def otherWire (Wire "other" 99))

(def loopedOther (Wire "otherLooped" :Looped 10))

(def funcWire
  (Wire
   "func"
   (If (-> (Is 10) (Or) (Is 20)) (Log))))

(def jumpWire
  (Wire
   "jumphere!"
   (Msg "jumped...")
   (Resume "ticked")))

(def jumpWire2
  (Wire
   "jumphere2!"
   (Msg "jumped...")
   (Resume)))

(def stopWire
  (Wire
   "stopping"
   (Msg "resumed stopping")
   (Stop)))

(def tickedWire
  (Wire
   "ticked"
   ; check that we can read root variables
   .var-from-root
   (Assert.Is "Ok!" true)
   
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
   false (Assert.Is true true)))

(def tickedWire2
  (Wire
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
  (Wire
   "started"
   (Msg "From top!")
   (Resume "root")
   ;; Should not enter here!
   false ; fail on purpose here
   (Assert.Is true true)))

(def main
  (Wire
   "root"
   "Initial input"
   (Once (Dispatch mywire))
   (Get "var1")
   (Log "var1")
   (Get "var2")
   (Log "var2")
   (Detach otherWire)
   (Wait otherWire)
   (Log "otherWire")
   (Assert.Is 99 true)
   10
   (Dispatch funcWire)
   12
   (Dispatch funcWire)
   20
   (Dispatch funcWire)

   (Detach loopedOther)
   (Stop loopedOther)
   (Wait loopedOther)

   "Ok!" = .var-from-root
                                        ; test a stepped wire that (Stop)s
   (Step tickedWire)
   (Msg "had message 1")
   (Assert.Is 1 true) ; pause after 1

   (Step tickedWire)
   (Msg "had message 2")
   (Assert.Is 1 true) ; resume pauses jumped

   (Step tickedWire)
   (Msg "before ticked resume")
   (Assert.Is 1 true) ; resumed ticked and resumed ticked again so paused

   (Step tickedWire) ; pause after 2
   (Assert.Is 2 true)

   (Step tickedWire) ; resume pauses when going stopping
   (Msg "had message 3")
   (Assert.Is 3 true) ; pause after 3

   (Step tickedWire)
   (Msg "had message 4")
   (Assert.Is 4 true) ; pause after 4

   (Step tickedWire) ; will stop the wire
   (Assert.Is 4 true)
   (Step tickedWire) ; last result wire is done
   (Assert.Is 4 true)
   (Step tickedWire) ; last result wire is done
   (Assert.Is 4 true)

                                        ; test a stepped wire that never stops and rotates
   (Repeat
    (->
     (Msg "repeating!")
     (Step tickedWire2)
     (Msg "had message 1")
     (Assert.Is 1 true) ; pause after 1

     (Step tickedWire2)
     (Msg "had message 2")
     (Assert.Is 1 true) ; resume pauses jumped

     (Step tickedWire2)
     (Msg "before ticked resume")
     (Assert.Is 1 true) ; resumed ticked and resumed ticked again so paused

     (Step tickedWire2) ; pause after 2
     (Assert.Is 2 true)

     (Step tickedWire2) ; resume pauses when going stopping
     (Msg "had message 3")
     (Assert.Is 3 true) ; pause after 3

     (Step tickedWire2)
     (Msg "had message 4")
     (Assert.Is 4 true) ; pause after 4
     ):Times 3)

   (Start startButNotResumed)
   (Msg "root resumed")
   (Start startButNotResumed)
   (Msg "root resumed")

   (Msg "done")))

(schedule root main)

(run root)

(schedule
 root
 (Wire
  "save"
  (Const main)
  (WriteFile "subwires.wire")
  (Msg "Serialized!")))

(run root)

(schedule
 root
 (Wire
  "load"
  (ReadFile "subwires.wire")
  (ExpectWire) >= .wire
  (Log "loaded")
  ;; We must do this here! cos .wire will try to resume self
  (WireRunner .wire :Mode RunWireMode.Detached)
  (Wait .wire)))

(run root)

(prn "DONE")

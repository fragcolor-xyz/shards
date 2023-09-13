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
   (SwitchTo "ticked")))

(def jumpWire2
  (Wire
   "jumphere2!"
   (Msg "jumped...")
   (SwitchTo)))

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
   (SwitchTo "jumphere!")
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
   (SwitchTo "stopping")
   ;; the flow stopped via Stop, the input of Stop was 4 from previous SwitchTo
   (Assert.Is 4 true)))

(def tickedWire2
  (Wire
   "ticked2"
   (Msg "message 1")
   1
   (Pause)
   (Msg "message 2")
                                        ; test jumping to another coro
   (SwitchTo "jumphere2!" true)
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
   (SwitchTo "root-wire")
   ; root-wire should complete, and so output "done"
   (Assert.Is "done" true)))

(def main
  (Wire
   "root-wire"
   "Initial input"
   (Once (Do mywire))
   (Get "var1")
   (Log "var1")
   (Get "var2")
   (Log "var2")
   (Detach otherWire)
   (Wait otherWire)
   (Log "otherWire")
   (Assert.Is 99 true)
   10
   (Do funcWire)
   12
   (Do funcWire)
   20
   (Do funcWire)

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

   (SwitchTo startButNotResumed true)
   (Msg "root resumed")
   (SwitchTo startButNotResumed true)
   (Msg "root resumed")

   "done"
   (Log)))

(schedule root main)

(if (run root) nil (throw "Root tick failed"))

(schedule
 root
 (Wire
  "save"
  (Const main)
  (WriteFile "subwires.wire")
  (Msg "Serialized!")))

(if (run root) nil (throw "Root tick failed"))

(schedule
 root
 (Wire
  "load"
  (ReadFile "subwires.wire")
  (ExpectWire) >= .wire
  (Log "loaded")
  ;; We must do this here! cos .wire will try to resume self
  (WireRunner .wire :Mode RunWireMode.Async)
  (Wait .wire)))

(if (run root) nil (throw "Root tick failed"))

(prn "DONE")

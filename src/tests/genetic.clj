; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Mesh))

(def fitness
  (Wire
   "fitness"
   (Math.Subtract 36)
   (ToFloat)
   (Math.Abs)
   (Math.Multiply -1.0)
   (Log "evolution happens here... fitness")))

(def evolveme
  (Wire
   "test"
   (Sequence .best :Types [Type.Float Type.Wire])
   (Repeat
    (->
     (Evolve
      (Wire
       "evolveme"
       (Mutant (Const 10) [0])
       (Pause)
       (Mutant (Math.Multiply 2) [0] [(->
                                       (RandomInt 10)
                                       (Math.Add 1))]))
      fitness
      :Population 64
      :Coroutines 8)
     (Log) > .best)
    15)
   .best
   (Take 1)
   (ToJson false)
   (Log)
   ))

(schedule Root evolveme)
(if (run Root 0.1) nil (throw "Root tick failed"))
(def Root nil)
(def evolveme nil)
(def fitness nil)
(prn "Done 1")

(def Root (Mesh))

(def fitness
  (Wire
   "fitness"
   (Math.Subtract 36)
   (ToFloat)
   (Math.Abs)
   (Math.Multiply -1.0)
   (Log "evolution happens here... fitness")))

(def state1
  (Wire
   "s1"
   (Msg "into state 1")
   (Start "s2")
   (Msg "back to state 1")
   (Resume "stepped")))

(def state2
  (Wire
   "s2"
   (Msg "into state 2")
   (Resume "s1")))

(def evolveme
  (Wire
   "test"
   (Sequence .best :Types [Type.Float Type.Wire])
   (Repeat
    (->
     (Evolve
      (Wire
       "evolveme"
       :Looped
       (Once (-> 0 >== .niters)) ;; global on purpose for testing
       (Math.Inc .niters)
       .niters
       (When (IsMore 10) (Stop))
       (Step (Wire "stepped" (Start state1)))
       (Mutant (Const 10) [0])
       (Mutant (Math.Multiply 2) [0] [(->
                                       (RandomInt 10)
                                       (Math.Add 1))]))
      fitness
      :Elitism 0.0
      :Population 100
      :Threads 10
      :Coroutines 1)
     (Log) > .best)
    4)
   .best
   (Log)
   (Take 1) (ExpectWire) >= .bestWire
   (WireRunner .bestWire :Mode RunWireMode.Detached)
   (Msg "Waiting...")
   (Wait .bestWire)
   (Msg "Exiting...")
   ))

(schedule Root evolveme)
(if (run Root 0.1) nil (throw "Root tick failed"))
(def Root nil)
(def evolveme nil)
(def fitness nil)
(prn "Done 2")

(def Root (Mesh))

(def fitness
  (Wire
   "fitness"
   (Math.Subtract 36)
   (ToFloat)
   (Math.Abs)
   (Math.Multiply -1.0)
   (Log "evolution happens here... fitness")))

(def evolveme
  (Wire
   "test"
   :Looped
   (Once (-> 0 >= .ntimes))
   (Math.Inc .ntimes)
   .ntimes
   (When (IsMore 7) (->
                     (Msg "STOP OK!")
                     nil (Stop)))
   (Evolve
    (Wire
     "evolveme"
     (Mutant (Const 10) [0])
     (Pause)
     (Mutant (Math.Multiply 2) [0] [(->
                                     (RandomInt 10)
                                     (Math.Add 1))]))
    fitness
    :Population 64
    :Coroutines 8)
   (Take 1)
   (ToJson false)
   (Log)
   nil))

(schedule Root evolveme)
(if (run Root 0.1) nil (throw "Root tick failed"))
(prn "Done 3")

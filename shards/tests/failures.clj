; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Mesh))

; fail during await, uses awaitne(...)
(def a
  (Wire
   "test-1"
   (Await (-> false (Assert.Is true)))))

(schedule Root a)
(if (run Root 0.1) (throw "Failure expected") nil)

; fail during http resolver, uses await(...)
(def c
  (Wire
   "test-2"
   (Http.Get "abc.abcdefg")))

(schedule Root c)
(if (run Root 0.1) (throw "Failure expected") nil)

; fail during cleanup, we cannot use pauses
; NOTE: Does not generate an error in `run`
(def c
  (Wire
   "test-3"
   (OnCleanup (-> (Pause 10.0)))))

(schedule Root c)
(if (run Root 0.1) nil (throw "Root tick failed"))

; out of range
(def c
  (Wire
   "test-3"
   [10 20 1000 40 50]
   (IntsToBytes)
   (Log)))

(schedule Root c)
(if (run Root 0.1) (throw "Failure expected") nil)

; wrong type
(def c
  (Wire
   "test-3"
   [10 20 2.0 40 50]
   (ExpectIntSeq)
   (IntsToBytes)
   (Log)))

(schedule Root c)
(if (run Root 0.1) (throw "Failure expected") nil)

; wrong type
(def c
  (Wire
   "test-3"
   [10 20 20 40 50]
   (ExpectIntSeq)
   (IntsToBytes)
   (ExpectLike [1 2 3])
   (Log)))

(schedule Root c)
(if (run Root 0.1) (throw "Failure expected") nil)

; fail the root wire propagated from Wait
(def d
  (Wire
   "test-4"
   (Detach a)
   (Wait a)
   (Assert.Is true false)))

(schedule Root d)
(if (run Root 0.1) (throw "Failure expected") nil)


; fail the root wire propagated from Wait
(def d
  (Wire
   "test-4"
   "4qjwieouqjweiouqweoi\") exit" (ParseFloat) (Log)))

(schedule Root d)
(if (run Root 0.1) (throw "Failure expected") nil)

(defmesh main)

(defloop c1
  .msg1 (Log))

(defloop c2
  .msg2 (Log))

(defloop cDead
  "Failed" (Fail))

(defloop c
  "Hello" = .msg1
  "World" = .msg2
  (Branch [c1 c2 cDead] BranchFailure.Known)
  (Msg "And Universe"))

(schedule Root c)
(if (run Root 0.2 25) (throw "Failure expected") nil)

(defloop c
  "Hello" = .msg1
  "World" = .msg2
  (Branch [c1 c2 cDead] BranchFailure.Ignore)
  (Msg "And Universe"))

(schedule Root c)
(if (run Root 0.2 25) nil (throw "Branch failure should be ignored"))

(defwire too-long-wire
  (Pause 120))

(defloop short-wire
  (Detach too-long-wire)
  (Maybe (-> (Wait too-long-wire :Timeout 1.0))
         (-> (Stop too-long-wire)
             "timed out" (Fail))))

(schedule Root short-wire)
(if (run Root 0.2 25) (throw "Failure expected") nil)

; Failure from Do should propagate to main wire
(defwire do-inner
  "Intentional fail" (Fail))
(defwire do-outer
  (Do do-inner)
  true (Assert.Is false))
(schedule Root do-outer)
(if (run Root 0.2 25) (throw "Failure expected from Do") nil)

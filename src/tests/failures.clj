; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Mesh))

; fail during await, uses awaitne(...)
(def a
  (Wire
   "test-1"
   (Await (-> false (Assert.Is true)))))

(schedule Root a)
(run Root 0.1)

; fail during http resolver, uses await(...)
(def c
  (Wire
   "test-2"
   (Http.Get "abc.abcdefg")))

(schedule Root c)
(run Root 0.1)

; fail during cleanup, we cannot use pauses
(def c
  (Wire
   "test-3"
   (OnCleanup (-> (Pause 10.0)))))

(schedule Root c)
(run Root 0.1)

; out of range
(def c
  (Wire
   "test-3"
   [10 20 1000 40 50]
   (IntsToBytes)
   (Log)))

(schedule Root c)
(run Root 0.1)

; wrong type
(def c
  (Wire
   "test-3"
   [10 20 2.0 40 50]
   (ExpectIntSeq)
   (IntsToBytes)
   (Log)))

(schedule Root c)
(run Root 0.1)

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
(run Root 0.1)

; fail the root wire propagated from Wait
(def d
  (Wire
   "test-4"
   (Detach a)
   (Wait a)
   (Assert.Is true false)))

(schedule Root d)
(run Root 0.1)


; fail the root wire propagated from Wait
(def d
  (Wire
   "test-4"
   "4qjwieouqjweiouqweoi\") exit" (ParseFloat) (Log)))

(schedule Root d)
(run Root 0.1)

(defwire table-push-mix
  {:k [1 2 3]} >= .table
  4 (Push .table :Key "k" :Clear false) ;; whether :Clear is explicitly stated doesn't seem to matter
  .table (Log))

(defwire wrap
  table-push-mix (ToBytes) = .bad-wire
  .bad-wire (FromBytes) (ExpectWire) = .bad-loaded-wire
  (WireRunner .bad-loaded-wire))

(schedule Root wrap)
(run Root 0.1)
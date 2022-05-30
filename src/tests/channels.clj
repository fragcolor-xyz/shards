; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Mesh))

(def producer
  (Wire
   "Producer"
   ;; :Looped
   (Repeat
    (-> "A message"
        (Produce "a")
        (Log "Produced: "))
    10)
   (Complete "a")))

(def consumer1
  (Wire
   "Consumer1"
   :Looped
   (Consume "a") ; consumes 1
   (Log "Consumed 1: ")))

(def consumer2
  (Wire
   "Consumer2"
   :Looped
   (Consume "a" 5)
   (Log "Consumed 2: ")))

(schedule Root producer)
(schedule Root consumer1)
(schedule Root consumer2)
(run Root 0.1)

(def producer
  (Wire
   "Producer"
   ;; :Looped
   (Repeat
    (-> "A message"
        (Broadcast "b" :NoCopy!! true)
        (Log "Broadcasted: ")
        (Pause 0.1))
    10)
   (Complete "b")))

(defn consumers [x]
  (Wire
   (str "Consumer" x)
   :Looped
   (Listen "b")
   (Log (str x ": "))))

(def consumer33
  (Wire
   "Consumer33"
   :Looped
   (Listen "b" 3)
   (Log "Consumed 33: ")))
(schedule Root producer)
(schedule Root (consumers 0))
(schedule Root (consumers 1))
(schedule Root (consumers 2))
(schedule Root consumer33)
(run Root 0.1)

(prn "Done")

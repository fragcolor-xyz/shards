(def Root (Node))

(def producer
  (Chain
   "Producer"
   ;; :Looped
   (Repeat
    (-->
     "A message"
     (Produce "a")
     (Log "Produced: ")
     (Sleep 0.1))
    10)
   (Complete "a")
   ))

(def consumer
  (Chain
   "Consumer"
   :Looped
   (Consume "a") ; consumes 1
   (Log "Consumed: ")))

(schedule Root producer)
(schedule Root consumer)
(run Root 0.1)

(def producer
  (Chain
   "Producer"
   ;; :Looped
   (Repeat
    (-->
     "A message"
     (Broadcast "b")
     (Log "Broadcasted: ")
     (Sleep 0.1))
    10)
   (Complete "b")
   ))

(defn consumers [x]
  (Chain
   (str "Consumer" x)
   :Looped
   (Listen "b")
   (Log (str x ": "))))

(schedule Root producer)
(schedule Root (consumers 0))
(schedule Root (consumers 1))
(schedule Root (consumers 2))
(run Root 0.1)

(def Root (Node))
(def Root (Node))

(def producer
  (Chain
   "Producer"
   ;; :Looped
   (Repeat
    (-->
     "A message"
     (Produce)
     (Log "Produced: ")
     (Sleep 0.1))
    10)
   (Complete)
   ))

(def consumer
  (Chain
   "Consumer"
   :Looped
   (Consume) ; consumes 1
   (Log "Consumed: ")))

(schedule Root producer)
(schedule Root consumer)
(run Root 0.1)

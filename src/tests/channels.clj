(def Root (Node))

(def producer
  (Chain
   "Producer"
   ;; :Looped
   (Repeat
    (-->
     "A message"
     (Produce))
    10)
   ;; (Complete "message")
   ))

(def consumer
  (Chain
   "Consumer"
   :Looped
   (Consume) ; consumes 1
   (Log "1: ")))

(schedule Root producer)
(schedule Root consumer)
(run Root 0.1)

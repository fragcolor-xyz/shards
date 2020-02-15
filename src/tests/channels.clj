(def Root (Node))

;; (def producer
;;   (Chain
;;    "Producer"
;;    ;; :Looped
;;    (Repeat
;;     (-->
;;      "A message"
;;      (Produce)
;;      (Log "Produced: ")
;;      (Sleep 0.1))
;;     10)
;;    (Complete)
;;    ))

;; (def consumer
;;   (Chain
;;    "Consumer"
;;    :Looped
;;    (Consume) ; consumes 1
;;    (Log "Consumed: ")))

;; (schedule Root producer)
;; (schedule Root consumer)
;; (run Root 0.1)

(def producer
  (Chain
   "Producer"
   ;; :Looped
   (Repeat
    (-->
     "A message"
     (Broadcast)
     (Log "Broadcasted: ")
     (Sleep 0.1))
    10)
   (Complete)
   ))

(defn consumers [x]
  (Chain
   (str "Consumer" x)
   :Looped
   (Listen)
   (Log (str x ": "))))

(schedule Root producer)
(schedule Root (consumers 0))
(schedule Root (consumers 1))
(schedule Root (consumers 2))
(run Root 0.1)

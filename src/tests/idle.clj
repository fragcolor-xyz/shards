(def Root (Node))

(def detect
  (Chain
   "detect"
   :Looped
   (Desktop.LastInput)
   (Log)))

(schedule Root detect)
(run Root 1.0)
(def Root (Node))

(def depth1Chain
  (Chain
   "d1" :Looped
   (Msg "D1 started")
   (Resume "d2")
   (Msg "D1 returned")
   (Resume "main")
   ))

(def depth2Chain
  (Chain
   "d2" :Looped
   (Msg "D2 started")
                                        ; go back
   (Resume "d1")
   (Msg "D2 returned")
   ))

(def mainChain
  (Chain
   "main" :Looped
   (Msg "Main started")
   (Resume "d1")
   (Msg "Main returned")
   ))

(schedule
 Root
 (Chain
  "root" :Looped
  (Msg "Root started")
  (Step "main")
  (Msg "Root returned")))

(run Root 0.1)

;; Root started
;;  Main started ; started by step
;;  D1 started ; started by continuechain!
;;  D2 started ; started by continuechain!
;;  D2 sleep
;; Root returned
;; Root started
;;  D1 returned
;;  D1 sleep
;; Root returned
;; Root started
;;  Main returned
;;  Main sleep (EoC)
;; Root returned
;; Root started
;;  Main started
;;  Main sleep
;; Root returned
;; Root started
;;  D1 sleep (EoC)



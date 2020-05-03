;; Not the best subject for chainblocks at all
;; but still if competitive in this would be nice

(def Root (Node))

(def - Math.Subtract)
(def + Math.Add)
(def < IsLess)

(def fib1
  (Chain
                                        ; chain header
   "fib"
                                        ; actual algo
   ;; (Log "enter")
   >= .n
   (If (< 2)
       :Then (Pass)
       :Else (--> .n (- 2) (Recur) >= .a
                  .n (- 1) (Recur) >= .b
                  .a (+ .b)))
   ;; (Log "exit")
   ))

(def fib2
  (Chain
                                        ; chain header
   "fib"
                                        ; actual algo
   ;; (Log "enter")
   >= .n
   (If (< 2)
       :Then (Pass)
       :Else (--> (- 2) (Recur) >= .a
                  .n (- 1) (Recur) (+ .a)))
   ;; (Log "exit")
   ))

(def fib3
  (Chain
                                        ; chain header
   "fib"
                                        ; actual algo
   ;; (Log "enter")
   (Push)
   (If (< 2)
       :Then (Pass)
       :Else (--> (- 2) (Recur) >= .a
                  (Pop) (- 1) (Recur) (+ .a)))
   ;; (Log "exit")
   ))

(schedule Root (Chain "run" 34 (Do fib3) (Log "result")))
(run Root)

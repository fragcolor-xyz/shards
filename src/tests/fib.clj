;; Not the best subject for chainblocks at all
;; but still if competitive in this would be nice

(def Root (Node))

(def - Math.Subtract)
(def + Math.Add)
(def < IsLess)

(def fib1
  (Chain
                                        ; chain header
   "fib1"
                                        ; actual algo
   ;; (Log "enter")
   >= .n
   (If (< 2)
       :Then (Pass)
       :Else (-> .n (- 2) (Recur) >= .a
                  .n (- 1) (Recur) >= .b
                  .a (+ .b)))
   ;; (Log "exit")
   ))

(def fib2
  (Chain
                                        ; chain header
   "fib2"
                                        ; actual algo
   ;; (Log "enter")
   >= .n
   (If (< 2)
       :Then (Pass)
       :Else (-> (- 2) (Recur) >= .a
                  .n (- 1) (Recur) (+ .a)))
   ;; (Log "exit")
   ))

(def fib3
  (Chain
                                        ; chain header
   "fib3"
                                        ; actual algo
   ;; (Log "enter")
   (Push)
   (If (< 2)
       :Then (Pass)
       :Else (-> (- 2) (Recur) >= .a
                  (Pop) (- 1) (Recur) (+ .a)))
   ;; (Log "exit")
   ))

(def fib4
  (Chain
   "fib4"
   (Math.Subtract 3) >= .n
   1 >= .a
   2 >= .b
   (Repeat
    ~[.a (Math.Add .b) >= .c
      .b > .a
      .c > .b]
    :Times .n)
   .b))

(schedule Root (Chain "run" (Profile (-> 34 (Do fib4))) (Log "result")))
(run Root)

(def fib1 nil)
(def fib2 nil)
(def fib3 nil)
(def Root nil)

(def Root (Node))

;; Notice, if running with valgrind:
;; you need valgrind headers and BOOST_USE_VALGRIND (-DUSE_VALGRIND @ cmake cmdline)
;; To run this properly or valgrind will complain

(def chain1
  (Chain
   "one"
   (Msg "one - 1")
   (Resume "two")
   (Msg "one - 2")
   (Msg "one - 3")
   (Resume "two")
   (Msg "one - Done")
   (Resume "two")
   ))

(def chain2
  (Chain
   "two"
   (Msg "two - 1")
   (Resume "one")
   (Msg "two - 2")
   (Msg "two - 3")
   (Resume "one")
   (Msg "two - 4")
   (Msg "two - Done")
   ))

(schedule Root chain1)
(run Root 0.1)

(def recursive
  (Chain
   "recur"
   (Log "depth")
   (Math.Inc)
   (Cond
    [(--> (IsLess 5))
     (Do "recur")])
   (Log "res")
   ))

(def logicChain
  (Chain
   "dologic"
   (IsMore 10)
   (Or)
   (IsLess 0)))

;; ;; Broken for now indeed, until we implement jumps

;; ;; (def recursiveAnd
;; ;;   (Chain
;; ;;    "recurAnd"
;; ;;    (Log "depth")
;; ;;    (Math.Inc)
;; ;;    (Push)
;; ;;    (IsLess 5)
;; ;;    (And)
;; ;;    (Pop)
;; ;;    (Do "recurAnd")
;; ;;    (Log "res")
;; ;;    ))

(schedule
 Root
 (Chain
  "doit"
  0
  (Do recursive)
  ;; (Do recursiveAnd)
  ))

;; test stack overflow, notice in this case (below) we could have tail call optimized,
;; TODO implement TCO

;; (def recursiveCrash
;;   (Chain
;;    "recurCrash"
;;    (Log "depth")
;;    (Math.Inc)
;;    (Do "recurCrash")
;;    ))

;; (schedule
;;  Root
;;  (Chain
;;   "doit"
;;   0
;;   (Do recursiveCrash)))

(def spawner
  (Chain
   "spawner"
   (Spawn logicChain)))

(def Loop
  (Chain
   "Loop" :Looped
   (Math.Inc)
   (Log)
   (Cond
    [(--> (Is 5))
     (Stop)])
   (Restart)))

(schedule
 Root
 (Chain
  "loop-test"
  0
  (Detach Loop)
  (Wait Loop)
  (Assert.Is 5 true)
  (Log)

  ;; test logic
  ;; ensure a sub inline chain
  ;; using Return mechanics
  ;; is handled by (If)
  -10
  (If (Do logicChain)
      (--> true)
      (--> false))
  (Assert.Is true false)

  -10
  (If (Do logicChain)
      (--> true)
      (--> false))
  (Assert.IsNot false false)

  11
  (If (Do logicChain)
      (--> true)
      (--> false))
  (Assert.Is true false)

  11
  (If (Do logicChain)
      (--> true)
      (--> false))
  (Assert.IsNot false false)

  0
  (If (Do logicChain)
      (--> true)
      (--> false))
  (Assert.Is false false)

  0
  (If (Do logicChain)
      (--> true)
      (--> false))
  (Assert.IsNot true false)

  (Const ["A" "B" "C"])
  (TryMany (Chain "print-stuff" (Log) "Ok"))
  (Assert.Is ["Ok" "Ok" "Ok"] false)
  (Const ["A" "B" "C"])
  (TryMany (Chain "print-stuff" (Log)) :Policy WaitUntil.FirstSuccess)
  (Assert.Is "A" false)

  -10
  (If ~[(Do spawner) >= .ccc (Wait .ccc) (ExpectBool)]
      (--> true)
      (--> false))
  (Assert.IsNot false false)

  11
  (If ~[(Do spawner) >= .ccc (Wait .ccc) (ExpectBool)]
      (--> true)
      (--> false))
  (Assert.Is true false)

  (Msg "Done")))

(run Root 0.1)

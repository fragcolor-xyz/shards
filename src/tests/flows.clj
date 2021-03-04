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
    [(-> (IsLess 5))
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
    [(-> (Is 5))
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
      (-> true)
      (-> false))
  (Assert.Is true false)

  -10
  (If (Do logicChain)
      (-> true)
      (-> false))
  (Assert.IsNot false false)

  11
  (If (Do logicChain)
      (-> true)
      (-> false))
  (Assert.Is true false)

  11
  (If (Do logicChain)
      (-> true)
      (-> false))
  (Assert.IsNot false false)

  0
  (If (Do logicChain)
      (-> true)
      (-> false))
  (Assert.Is false false)

  0
  (If (Do logicChain)
      (-> true)
      (-> false))
  (Assert.IsNot true false)

  "Hello world" = .hello-var

  (Const ["A" "B" "C"])
  (TryMany (Chain "print-stuff" (Log) .hello-var (Log) "Ok"))
  (Assert.Is ["Ok" "Ok" "Ok"] false)
  (Const ["A" "B" "C"])
  (TryMany (Chain "print-stuff" (Log) .hello-var (Log) "A") :Policy WaitUntil.FirstSuccess)
  (Assert.Is "A" false)

  (Const ["A" "B" "C"])
  (TryMany (Chain "print-stuff" (Log) "Ok") :Threads 3)
  (Assert.Is ["Ok" "Ok" "Ok"] false)
  (Const ["A" "B" "C"])
  (TryMany (Chain "print-stuff" (Log) "A") :Threads 3 :Policy WaitUntil.FirstSuccess)
  (Assert.Is "A" false)

  (Repeat (-> 10
              (Expand 10 (defchain wide-test (Math.Inc)) :Threads 10)
              (Assert.Is [11 11 11 11 11 11 11 11 11 11] true)
              (Log))
          :Times 10)
  
  (Repeat (-> 10
              (Expand 10 (defchain wide-test (RandomBytes 8) (ToHex)) :Threads 10)
              (Log))
          :Times 10)

  10
  (Expand 10 (defchain wide-test (Math.Inc)))
  (Assert.Is [11 11 11 11 11 11 11 11 11 11] true)
  (Log)

  -10
  (If ~[(Do spawner) >= .ccc (Wait .ccc) (ExpectBool)]
      (-> true)
      (-> false))
  (Assert.IsNot false false)

  11
  (If ~[(Do spawner) >= .ccc (Wait .ccc) (ExpectBool)]
      (-> true)
      (-> false))
  (Assert.Is true false)

  (Msg "Done")))

(run Root 0.1)

(def test-case-step
  (Chain
   "test-case-step"
   :Looped
   (Get "x" :Default 0)
   (Math.Inc) >= .x
   (Log "x")))

(prepare test-case-step)
(start test-case-step)
(tick test-case-step)
(tick test-case-step)

(schedule
 Root
 (Chain
  "continue-stepping"
  :Looped
  (Step test-case-step)
  (Assert.Is 3 true)
  (Msg "Done")
  (Stop)))

(run Root 0.1)

(if (hasBlock? "Http.Post")
  (do
    (defchain upload-to-ipfs
      (let [boundary "----CB-IPFS-Upload-0xC0FFEE"
            gateways ["https://ipfs.infura.io:5001"
                      "https://ipfs.komputing.org"
                      "http://hasten-ipfs.local:5001"
                      "http://127.0.0.1:5001"]]
        (->
         >= .payload
         (str "--" boundary "\r\nContent-Disposition: form-data; name=\"path\"\r\nContent-Type: application/octet-stream\r\n\r\n")
         (PrependTo .payload)
         (str "\r\n--" boundary "--")
         (AppendTo .payload)
         gateways
         (TryMany (Chain "IPFS-Upload"
                         >= .gateway
                         "/api/v0/add?pin=true" (AppendTo .gateway)
                         .payload
                         (Http.Post .gateway
                                    :Headers {"Content-Type" (str "multipart/form-data; boundary=" boundary)}))
                  :Policy WaitUntil.SomeSuccess)
         (Take 0) (FromJson) (ExpectTable)
         (Take "Hash") (ExpectString)
         (Assert.Is "QmNRCQWfgze6AbBCaT1rkrkV5tJ2aP4oTNPb5JZcXYywve" true))))

    (defchain test-ipfs
      "Hello world" (Do upload-to-ipfs) (Log "ipfs hash"))

    (schedule Root test-ipfs)
    (run Root 0.1)))

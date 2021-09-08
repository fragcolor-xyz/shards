; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

(def myChain
  (Chain
   "myChain"
   (Msg "My chain here!")))

(def! n (Node))
(schedule n
          (Chain
           "n" :Looped
           10 (Set "var")
           (ChainLoader (Chain* "loadme.clj"
                                ; bootstrap code into the loader environment
                                '(def defined-text2 "Hello world 2!")
                                ))))

(def! dec (fn* [a] (- a 1)))
(def! Loop (fn* [count] (do
                          (if (tick n) nil (throw "tick failed"))
                          (sleep 0.1)
                          (if (> count 0) (Loop (dec count)) nil))))

(Loop 30)

(prn "done...")

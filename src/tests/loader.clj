; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

(def myWire
  (Wire
   "myWire"
   (Msg "My wire here!")))

(def! n (Mesh))
(schedule n
          (Wire
           "n" :Looped
           10 (Set "var")
           (WireLoader (Wire* "loadme.clj"
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

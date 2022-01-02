; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def! n (Node))

(schedule
 n
 (Chain
  "n"
  "" (Process.Run "echo" ["Hello world"]) (Log)
  (Assert.Is "Hello world\n" true)
  "" (Process.Run "echo" ["Hello world"]) (Log)
  (Assert.Is "Hello world\n" true)

  "Hello world" (Process.Run "cat" :Timeout 2) (Log)
  (Assert.Is "Hello world\n" true)

  (Maybe (-> "" (Process.Run "sleep" ["10"] :Timeout 1) (Log)))))

;; (def! dec (fn* [a] (- a 1)))
;; (def! Loop (fn* [count] (do
;;   (if (tick n) nil (throw "tick failed"))
;;   (sleep 0.5)
;;   (if (> count 0) (Loop (dec count)) nil)
;; )))

;; (Loop 5)

(run n 0.1)
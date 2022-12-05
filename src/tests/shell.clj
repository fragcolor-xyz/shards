; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def! n (Mesh))

(schedule
 n
 (Wire
  "n"
  "" (Process.Run "echo" ["Hello world"]) (Log)
  (Assert.Is "Hello world\n" true)
  "" (Process.Run "echo" ["Hello world"]) (Log)
  (Assert.Is "Hello world\n" true)

  ["10"] = .args
  (Maybe (-> "" (Process.Run "sleep" .args :Timeout 1) (Log)))))

(if (run n 0.1) nil (throw "Root tick failed"))
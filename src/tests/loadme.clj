; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

(def defined-text1 "Hello world!")
(Get "var")
(Math.Add 1)
(Log)
(Msg "Hello")
(Time.Delta)
(Log "DT")
(Detach "myChain")
(eval (read-string (str "(-> " (slurp "loadme-extra.clj") ")")))

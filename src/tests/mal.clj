; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

(def wire (Wire "test" true))
(def shard1 (Do wire))
(prn "Expect test wire deleted under")
(def shard1 nil)
(def wire nil)

(def shard2 (Do (Wire "inline-wire" true)))
(prn "Expect inline-wire wire deleted under")
(def shard2 nil)

(Wire "lost-wire" true)
(prn "Expect lost-wire wire deleted above")
(do (Wire "do-wire" true))
(prn "Expect do-wire wire deleted above")

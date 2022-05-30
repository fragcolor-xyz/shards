; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2021 Fragcolor Pte. Ltd.

;; (prn (shards))
(prn (map (fn* [name] [name (info name)]) (shards)))
(prn (export-strings))

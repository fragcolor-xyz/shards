; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2021 Fragcolor Pte. Ltd.

;; (prn (shards))
(prn (map (fn* [name] [name (shard-info name)]) (shards)))
(prn (map (fn* [name] [name (enum-info name)]) (enums)))
(prn (export-strings))

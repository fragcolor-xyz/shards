; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2021 Fragcolor Pte. Ltd.

;; (prn (blocks))
(prn (map (fn* [name] [name (info name)]) (blocks)))
(prn (export-strings))

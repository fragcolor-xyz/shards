; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

(def! root (Node))
(schedule root (Chain "crash" 10 (Assert.Is 1 true)))
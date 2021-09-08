; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Node))
(schedule Root (Chain
                "py1"
                :Looped
                10
                (Py "PyBlock1" "Hello")
                (Assert.Is 20 true)
                (Math.Add 20)
                (Assert.Is 40 true)
                (Log)))

(schedule Root (Chain
                "py2"
                :Looped
                20
                (Py "PyBlock1")
                (Assert.Is 30 true)
                (Math.Add 20)
                (Assert.Is 50 true)
                (Log)))

(schedule Root (Chain
                "py3"
                :Looped
                (Py "PyBlock2")
                (Assert.Is ["Hello" "World"] true)
                (Log)))

(run Root 1.0 5)

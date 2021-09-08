; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

(def Root (Node))

(schedule
 Root
 (Chain
  "test"

  (Const [10 20 3.14])
  (Pack "i32 i32 f32")
  (Set "bytes")
  (Log)
  
  (Get "bytes")
  (Unpack "i32 i32 f32")
  (Log)
  (ExpectSeq) &> .bseq
  (Take 1)
  (ExpectInt)
  (Assert.Is 20 true)
  .bseq (Take 2)
  (Assert.Is 3.14 true)

  (Const ["Hello structured data" 2])
  (Pack "s i32")
  (Log)
  (Unpack "s i32")
  (Log)
  (Take 0)
  (ExpectString)
  (Log)
  (Assert.Is "Hello structured data" true)

  (Const [1.0 2.0 3.0]) >> .args
  22 >> .args
  .args
  (Pack "f32[3] p")
  (Log)
  (Unpack "f32[3] p") >= .x
  (Take 1)
  (ExpectInt)
  (Assert.Is 22 true)
  .x (Take 0) (ExpectSeq)
  (Log)
  (Take 2)
  (ExpectFloat)
  (Assert.Is 3.0 true)

  (Const [1 2 3]) >> .args2
  22 >> .args2
  .args2
  (Pack "i64[3] p")
  (Log)
  (Unpack "i64[3] p") >= .y
  (Take 1)
  (ExpectInt)
  (Assert.Is 22 true)
  .y (Take 0) (ExpectSeq)
  (Log)
  (Take 2)
  (ExpectInt)
  (Assert.Is 3 true)
  ))

(tick Root)

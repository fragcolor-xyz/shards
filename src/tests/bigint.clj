; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Mesh))

(def test
  (Wire
   "bigints"
   :Looped
   ; big ints
   1000 (BigInt) (BigInt.Shift :By 18) >= .1000x1e18
   (BigInt.ToString) (Log "bigint")
   .1000x1e18 (BigInt.ToFloat :ShiftedBy -18) (Log "bigint as shifted float")
   (Assert.Is 1000.0 true)
   "2" (BigInt) >= .2
   "500000000000000000000" (BigInt) >= .500x1e18 (BigInt.ToFloat :ShiftedBy -18)
   (Assert.Is 500.0 true)
   .1000x1e18 (BigInt.Multiply .2) (BigInt.ToFloat :ShiftedBy -18)
   (Assert.Is 2000.0 true)
   .1000x1e18 (BigInt.Add .500x1e18) (BigInt.ToFloat :ShiftedBy -18)
   (Assert.Is 1500.0 true)
   .1000x1e18 (BigInt.Subtract .500x1e18) (BigInt.ToFloat :ShiftedBy -18)
   (Assert.Is 500.0 true)

   .1000x1e18 >> .bigseq
   .1000x1e18 >> .bigseq
   .1000x1e18 >> .bigseq
   .bigseq (BigInt.Multiply .2) (Log)
   (Take 0)
   (| (BigInt.ToHex) (Log))
   (BigInt.ToBytes :Bits 256) (Log)
   (ToHex) (Log)
   (Assert.Is "0x00000000000000000000000000000000000000000000006c6b935b8bbd400000" true)

   "18446744073709551615" (BigInt) (BigInt.ToBytes 64) = .max-u64
   (BigInt) (BigInt.ToString) (Assert.Is "18446744073709551615" true) (Log "Returned")
   .max-u64 (Substrate.Decode [Type.Int] ["u64"]) (Log "Decoded")

   ;
   ))


(schedule Root test)
(run Root 0.1 10)
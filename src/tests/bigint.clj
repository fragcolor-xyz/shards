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

   "4e2" (HexToBytes) (BigInt) (BigInt.ToString) (Assert.Is "1250" true) (Log "Returned")

   ; Comparisons
   .500x1e18 (BigInt.Is .500x1e18) (Assert.Is true)
   .1000x1e18 (BigInt.Is .500x1e18) (Assert.Is false)

   .500x1e18 (BigInt.IsNot .500x1e18) (Assert.Is false)
   .1000x1e18 (BigInt.IsNot .500x1e18) (Assert.Is true)

   .1000x1e18 (BigInt.IsMore .500x1e18) (Assert.Is true)
   .500x1e18 (BigInt.IsMore .1000x1e18) (Assert.Is false)
   .500x1e18 (BigInt.IsMore .500x1e18) (Assert.Is false)

   .1000x1e18 (BigInt.IsLess .500x1e18) (Assert.Is false)
   .500x1e18 (BigInt.IsLess .1000x1e18) (Assert.Is true)
   .500x1e18 (BigInt.IsLess .500x1e18) (Assert.Is false)

   .1000x1e18 (BigInt.IsMoreEqual .500x1e18) (Assert.Is true)
   .500x1e18 (BigInt.IsMoreEqual .1000x1e18) (Assert.Is false)
   .500x1e18 (BigInt.IsMoreEqual .500x1e18) (Assert.Is true)

   .1000x1e18 (BigInt.IsLessEqual .500x1e18) (Assert.Is false)
   .500x1e18 (BigInt.IsLessEqual .1000x1e18) (Assert.Is true)
   .500x1e18 (BigInt.IsLessEqual .500x1e18) (Assert.Is true)

   100 (BigInt) (BigInt.Pow 20) (Log "100^20")
   (| (BigInt.ToString) (Assert.Is "10000000000000000000000000000000000000000" true) (Log "Result"))
   (| (BigInt.Sqrt) (BigInt.ToString) (Assert.Is "100000000000000000000" true) (Log "Sqrt"))

   ;
   ))


(schedule Root test)
(run Root 0.1 10)
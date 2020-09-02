(def Root (Node))

(def test
  (Chain
   "bigints"
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
   (Assert.Is 500.0 true)))

(schedule Root test)
(run Root 0.1)
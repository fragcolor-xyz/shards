(def! node (Node))

(schedule
 node
 (Chain
  "variables" :Looped
  10
  (Set "x")
  (Assert.Is 10 true)
  20
  (Assert.Is 20 true)
  (Get "x")
  (Assert.Is 10 true)
  
  (Set "y" "key1")
  (Assert.Is 10 true)
  (Get "y")
  (Assert.IsNot 10 true)
  (Log)
  (Get "y" "key1")
  (Assert.Is 10 true)
  (Log)
  77
  (Set "y" "key2")
  (Assert.Is 77 true)
  20
  (Assert.Is 20 true)
  (Get "y" "key1")
  (Assert.Is 10 true)
  (Get "y" "key2")
  (Assert.Is 77 true)

  9
  (Set "a")
  18
  (Set "b")
  (Get "a")
  (Assert.Is 9 true)
  (Get "b")
  (Assert.Is 18 true)
  (Swap "a" "b")
  (Get "a")
  (Assert.Is 18 true)
  (Get "b")
  (Assert.Is 9 true)
  (ToString)
  (Set "as")
  (Assert.Is "9" true)
  (Assert.IsNot 9 true)

  1
  (Push "e" :Clear false)
  2
  (Push "e")
  3
  (Push "e")
  4
  (Push "e")
  5
  (Push "e")
  (Get "e")
  (Assert.Is [1 2 3 4 5] true)
  (Log)

  1
  (Push "e")
  2
  (Push "e")
  3
  (Push "e")
  4
  (Push "e")
  5
  (Push "e")
  (Get "e")
  (Assert.Is [1 2 3 4 5 1 2 3 4 5] true)
  (Log)

  (Remove .e :Predicate (IsMore 2) :Unordered false)
  (Assert.Is [1 2 1 2] true)
  (Log)

  2
  (Push .e)
  (Get .e)
  (Assert.Is [1 2 1 2 2] true)
  (Log)

  (Remove .e :Predicate (IsLessEqual 2) :Unordered false)
  (Assert.Is [] true)
  (Log)

  2
  (Push .e)
  (Get .e)
  (Assert.Is [2] true)
  (Log)

  (Remove .e :Predicate (IsLessEqual 2) :Unordered false)
  (Assert.Is [] true)
  (Log)

  (Const [6 7 8 9])
  (Set "c")
  (Log)
  (Get "c")
  (Assert.Is [6 7 8 9] true)
  (Take [0 1])
  (Assert.Is [6 7] true)
  (Log)
  (Take 0)
  (Assert.Is 6 true)
  (Log)

  (Const ["a" "b"])
  (Set "d" "k1")
  "c"
  (Push "d" "k1")
  (Get "d" "k1")
  (Assert.Is ["a" "b" "c"] true)

  (Get .d)
  (Push .nested :Clear false)

  1111
  (Push .d "k2")
  (Get .d)
  (Push .nested)

  1112
  (Push .d "k2")
  (Get .d)
  (Push .nested)

  (Get .nested)
  (Log "nested")
  (Remove .nested :Predicate (Const true) :Unordered true)
  (Assert.Is [] true)
  (Log)

  (Const [1 2 3])
  (Push .n1)
  (Const [2 3 4])
  (Push .n1)
  (Const [3 4 5])
  (Push .n1)
  (Get .n1)
  (Log "before U remove")
  (Remove .n1 :Predicate (--> (Take 0) (IsLess 3)) :Unordered true)
  (Log "after U remove")
  (Assert.Is [[3 4 5]] true)
  (Count .n1)
  (Log "count U")

  (Const [1 2 3])
  (Push .n2)
  (Const [2 3 4])
  (Push .n2)
  (Const [3 4 5])
  (Push .n2)
  (Get .n2)
  (Log "before O remove")
  (Remove .n2 :Predicate (--> (Take 0) (IsLess 3)) :Unordered false)
  (Log "after O remove")
  (Assert.Is [[3 4 5]] true)
  (Count .n2)
  (Log "count O")

  "global1"
  (Set .global1 :Global true)
  (Get .global1)
  (Assert.Is "global1" true)

  (Msg "Done!")
  ))

(tick node)
(tick node)
(tick node)
(tick node)

(prn "Done!")

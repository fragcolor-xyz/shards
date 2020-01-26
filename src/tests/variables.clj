(def! node (Node))

(schedule node (Chain "variables" :Looped
  10
  (Set "x")
  (Assert.Is 10 true)
  20
  (Assert.Is 20 true)
  (Get "x")
  (Assert.Is 10 true)
  (Set "x" "key1") ; we overwrite "x" and turn it into a table
  (Assert.Is 10 true)
  (Get "x")
  (Assert.IsNot 10 true)
  (Log)
  (Get "x" "key1")
  (Assert.Is 10 true)
  (Log)
  77
  (Set "x" "key2")
  (Assert.Is 77 true)
  20
  (Assert.Is 20 true)
  (Get "x" "key1")
  (Assert.Is 10 true)
  (Get "x" "key2")
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
  (Set "a")
  (Assert.Is "9" true)
  (Assert.IsNot 9 true)

  1
  (Push "a")
  2
  (Push "a")
  3
  (Push "a")
  4
  (Push "a")
  5
  (Push "a")
  (Get "a")
  (Assert.Is [1 2 3 4 5] true)
  (Log)

  1
  (Push "a")
  2
  (Push "a")
  3
  (Push "a")
  4
  (Push "a")
  5
  (Push "a")
  (Get "a")
  (Assert.Is [1 2 3 4 5 1 2 3 4 5] true)
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
))

(tick node)

(prn "Done!")

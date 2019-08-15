(def! node (Node))

(schedule node (Chain "variables"
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
))

(prn "Done!")
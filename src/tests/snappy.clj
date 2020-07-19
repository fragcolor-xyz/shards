(def Root (Node))

(schedule
 Root
 (Chain
  "snappy-test"
  "Compressing this string is the test, Compressing this string is the test"
  (Set "string")
  (Count "string")
  (Log)
  (Get "string")
  (ToBytes)
  (Snappy.Compress)
  (Set "compressed")
  (Count "compressed")
  (Log)
  (Get "compressed")
  (Snappy.Decompress)
  (ExpectString)
  (Assert.Is "Compressing this string is the test, Compressing this string is the test" true)
  (Log)))

(tick Root)

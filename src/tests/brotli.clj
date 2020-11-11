(def Root (Node))

(schedule
 Root
 (Chain
  "brotli-test"
  "Compressing this string is the test, Compressing this string is the test"
  (Set "string")
  (Count "string")
  (Log "length")
  (Get "string")
  (ToBytes)
  (Brotli.Compress :Quality 7)
  (Set "compressed")
  (Count "compressed")
  (Log "compressed")
  (Get "compressed")
  (Brotli.Decompress)
  (FromBytes)
  (ExpectString)
  (Assert.Is "Compressing this string is the test, Compressing this string is the test" true)
  (Log)))

(tick Root)

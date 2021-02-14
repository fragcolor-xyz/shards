(def Root (Node))

(schedule
 Root
 (Chain
  "test"
  "Hello world"
  (Hash.Keccak256)
  (ToHex)
  (Assert.Is "ed6c11b0b5b808960df26f5bfc471d04c1995b0ffd2055925ad1be28d6baadfd" true)
  (Log)))

(run Root)

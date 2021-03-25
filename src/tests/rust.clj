(def Root (Node))

(schedule
 Root
 (Chain
  "test"
  ""
  (| (Hash.Sha2-256) (ToHex) (Assert.Is "0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"))
  (| (Hash.Sha2-512) (ToHex) (Assert.Is "0xcf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"))
  (| (Hash.Sha3-256) (ToHex) (Assert.Is "0xa7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"))
  (| (Hash.Sha3-512) (ToHex) (Assert.Is "0xa69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26"))
  "Hello world"
  (| (ToBase58) (Log)
     (FromBase58) (BytesToString) (Log)
     (Assert.Is "Hello world" true))
  (Hash.Keccak-256) = .hash
  (ToHex)
  (Assert.Is "0xed6c11b0b5b808960df26f5bfc471d04c1995b0ffd2055925ad1be28d6baadfd" true)
  .hash (Sign.ECDSA "1536f1d756d1abf83aaf173bc5ee3fc487c93010f18624d80bd6d4038fadd59e")
  (ToHex) (Log)))

(run Root)

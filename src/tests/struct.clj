(def Root (Node))

(schedule
 Root
 (Chain
  "test"

  (Const [10 20 3.14])
  (Pack "i32 i32 f64")
  (Set "bytes")
  (Log)
  (BytesToInt32)
  (Log)
  (Get "bytes")
  (BytesToFloat64)
  (Log)
  (Get "bytes")
  (Unpack "i32 i32 f64")
  (Log)
  ))

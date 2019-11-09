(def Root (Node))

(schedule
 Root
 (Chain
  "test"

  (Const [10 20 3.14])
  (Pack "i32 i32 f32")
  (Set "bytes")
  (Log)
  (BytesToInt32)
  (Take 1)
  (Assert.Is 20 true)
  (Get "bytes")
  (BytesToFloat32)
  (Take 2)
  (Assert.Is 3.14 true)
  (Get "bytes")
  (Unpack "i32 i32 f32")
  (Log)
  ))

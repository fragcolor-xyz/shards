(def Root (Node))

(schedule
 Root
 (Chain
  "test"

  (Const [10 20])
  (Pack "i32 i32 i8[256]")
  (Log)
  ))

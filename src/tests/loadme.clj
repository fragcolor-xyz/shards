                                        ; must return a chain as script result
(do
  (Chain "loaded"
         (Get "var")
         (Math.Add 1)
         (Log)
         (Msg "Hello")
         (Detach "myChain")))


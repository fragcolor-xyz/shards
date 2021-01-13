                                        ; must return a chain as script result
(do
  (def defined-text1 "Hello world!")

  (Chain "loaded"
         (Get "var")
         (Math.Add 1)
         (Log)
         (Msg "Hello")
         (Time.Delta)
         (Log "DT")
         (Detach "myChain")
         (eval (read-string (str "~[" (slurp "loadme-extra.clj") "]")))))


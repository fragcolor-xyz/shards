(def Root (Node))

(def test
  (Chain
   "test"
   :Looped
   "" (Wasm.Run "../../deps/wasm3/test/wasi/test.wasm") (Log "r1")
   "" (Wasm.Run "../../deps/wasm3/test/wasi/test.wasm" ["cat" "wasm.clj"]) (Log "r2")
   "" (Wasm.Run "../../deps/wasm3/test/lang/fib32.wasm" ["10"] :EntryPoint "fib") (Log "r3")
   "" (Wasm.Run "../../deps/wasm3/test/lang/fib32.wasm" ["20"] :EntryPoint "fib") (Log "r4")
   ))

(schedule Root test)
(run Root 0.1 10)
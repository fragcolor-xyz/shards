(def Root (Node))

(def test
  (Chain
   "test"
   :Looped
   "" (Wasm.Run "../../deps/wasm3/test/wasi/test.wasm")
   "" (Wasm.Run "../../deps/wasm3/test/wasi/test.wasm" ["cat" "../src/tests/wasm.clj"])
   "" (Wasm.Run "../../deps/wasm3/test/lang/fib32.wasm" ["10"] :EntryPoint "fib")
   "" (Wasm.Run "../../deps/wasm3/test/lang/fib32.wasm" ["20"] :EntryPoint "fib")
   ))

(schedule Root test)
(run Root 0.1 10)
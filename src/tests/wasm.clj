(def Root (Node))

(def test
  (Chain
   "test"
   :Looped
   "" (Wasm.Run "../../deps/wasm3/test/wasi/test1.wasm")
   "" (Wasm.Run "../../deps/wasm3/test/wasi/test1.wasm")
  ;;  "" (Wasm.Run "../../deps/wasm3/test/lang/fib32.wasm" :EntryPoint "fib")
  ;;  "" (Wasm.Run "../../deps/wasm3/test/lang/fib32.wasm" :EntryPoint "fib")
   ))

(schedule Root test)
(run Root 0.1 10)
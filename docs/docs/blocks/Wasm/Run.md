---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Wasm.Run

```clojure
(Wasm.Run
  :Module [(Path) (String)]
  :Arguments [(None) (Seq [(String)]) (ContextVar [(Seq [(String)])])]
  :EntryPoint [(String)]
  :StackSize [(Int)]
  :ResetRuntime [(Bool)]
  :CallConstructors [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Module | `[(Path) (String)]` | `""` | The wasm module to run. |
| Arguments | `[(None) (Seq [(String)]) (ContextVar [(Seq [(String)])])]` | `None` | The arguments to pass to the module entrypoint function. |
| EntryPoint | `[(String)]` | `"_start"` | The entry point function to call when activating. |
| StackSize | `[(Int)]` | `1024` | The stack size in kilobytes to use. |
| ResetRuntime | `[(Bool)]` | `true` | If the runtime should be reset every activation, altho slow this might be useful if certain modules fail to execute properly or leak on multiple activations. |
| CallConstructors | `[(Bool)]` | `false` | Use if it might be necessary to force a call to `__wasm_call_dtors`, modules generated with WASI rust might need this. |


## Input

| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Examples

```clojure
(Wasm.Run

)
```


--8<-- "includes/license.md"

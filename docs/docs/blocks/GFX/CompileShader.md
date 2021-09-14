---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# GFX.CompileShader

```clojure
(GFX.CompileShader
  :Type [(Enum)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Type | `[(Enum)]` | `Enum: 0 vendor: 0x73696e6b type: 0x67667853` | The type of shader to compile. |


## Input

| Type | Description |
|------|-------------|
| `[(Table {"varyings" (String) "code" (String) "defines" (Seq [(String)])})]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Bytes)]` |  |


## Examples

```clojure
(GFX.CompileShader

)
```

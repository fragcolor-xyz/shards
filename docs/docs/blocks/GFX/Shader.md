---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# GFX.Shader

```clojure
(GFX.Shader
  :VertexShader [(Bytes) (ContextVar [(Bytes)])]
  :PixelShader [(Bytes) (ContextVar [(Bytes)])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| VertexShader | `[(Bytes) (ContextVar [(Bytes)])]` | `None` | The vertex shader bytecode. |
| PixelShader | `[(Bytes) (ContextVar [(Bytes)])]` | `None` | The pixel shader bytecode. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Object)]` |  |


## Examples

```clojure
(GFX.Shader

)
```


--8<-- "includes/license.md"

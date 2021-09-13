---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.MatMul

```clojure
(Math.MatMul
  :Operand [(Float2) (Seq [(Float2)]) (Float3) (Seq [(Float3)]) (Float4) (Seq [(Float4)]) (ContextVar [(Float2)]) (ContextVar [(Seq [(Float2)])]) (ContextVar [(Float3)]) (ContextVar [(Seq [(Float3)])]) (ContextVar [(Float4)]) (ContextVar [(Seq [(Float4)])])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Operand | `[(Float2) (Seq [(Float2)]) (Float3) (Seq [(Float3)]) (Float4) (Seq [(Float4)]) (ContextVar [(Float2)]) (ContextVar [(Seq [(Float2)])]) (ContextVar [(Float3)]) (ContextVar [(Seq [(Float3)])]) (ContextVar [(Float4)]) (ContextVar [(Seq [(Float4)])])]` | `0` | The operand. |


## Input

| Type | Description |
|------|-------------|
| `[(Float2) (Seq [(Float2)]) (Float3) (Seq [(Float3)]) (Float4) (Seq [(Float4)])]` | Any valid integer(s) or floating point number(s) supported by this operation. |


## Output

| Type | Description |
|------|-------------|
| `[(Float2) (Seq [(Float2)]) (Float3) (Seq [(Float3)]) (Float4) (Seq [(Float4)])]` | The result of the operation, usually in the same type as the input value. |


## Examples

```clojure
(Math.MatMul

)
```

---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Min

```clojure
(Min
  :Operand [(Int) (ContextVar [(Int)]) (Int2) (ContextVar [(Int2)]) (Int3) (ContextVar [(Int3)]) (Int4) (ContextVar [(Int4)]) (Int8) (ContextVar [(Int8)]) (Int16) (ContextVar [(Int16)]) (Float) (ContextVar [(Float)]) (Float2) (ContextVar [(Float2)]) (Float3) (ContextVar [(Float3)]) (Float4) (ContextVar [(Float4)]) (Color) (ContextVar [(Color)]) (Seq [(Any)]) (ContextVar [(Seq [(Any)])])]
)
```


## Definition

Applies the binary operation on the input value and operand and returns its result, or a sequence of results if input and operand are sequences.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Operand | `[(Int) (ContextVar [(Int)]) (Int2) (ContextVar [(Int2)]) (Int3) (ContextVar [(Int3)]) (Int4) (ContextVar [(Int4)]) (Int8) (ContextVar [(Int8)]) (Int16) (ContextVar [(Int16)]) (Float) (ContextVar [(Float)]) (Float2) (ContextVar [(Float2)]) (Float3) (ContextVar [(Float3)]) (Float4) (ContextVar [(Float4)]) (Color) (ContextVar [(Color)]) (Seq [(Any)]) (ContextVar [(Seq [(Any)])])]` | `0` | The operand. |


## Input

| Type | Description |
|------|-------------|
| `[(Int) (Int2) (Int3) (Int4) (Int8) (Int16) (Float) (Float2) (Float3) (Float4) (Color) (Seq [(Any)])]` | Any valid integer(s) or floating point number(s) supported by this operation. |


## Output

| Type | Description |
|------|-------------|
| `[(Int) (Int2) (Int3) (Int4) (Int8) (Int16) (Float) (Float2) (Float3) (Float4) (Color) (Seq [(Any)])]` | The result of the operation, usually in the same type as the input value. |


## Examples

```clojure
(Const [-1 0 1 2 5])
(Min
 ;:Operand
 3)
(Assert.Is [-1 0 1 2 3] true)
```

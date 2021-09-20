---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Inc

```clojure
(Math.Inc
  :Value [(ContextVar [(Int)]) (ContextVar [(Int2)]) (ContextVar [(Int3)]) (ContextVar [(Int4)]) (ContextVar [(Int8)]) (ContextVar [(Int16)]) (ContextVar [(Float)]) (ContextVar [(Float2)]) (ContextVar [(Float3)]) (ContextVar [(Float4)]) (ContextVar [(Color)]) (ContextVar [(Seq [(Any)])])]
)
```


## Definition

Applies the binary operation on the input value and operand and returns its result, or a sequence of results if input and operand are sequences.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Value | `[(ContextVar [(Int)]) (ContextVar [(Int2)]) (ContextVar [(Int3)]) (ContextVar [(Int4)]) (ContextVar [(Int8)]) (ContextVar [(Int16)]) (ContextVar [(Float)]) (ContextVar [(Float2)]) (ContextVar [(Float3)]) (ContextVar [(Float4)]) (ContextVar [(Color)]) (ContextVar [(Seq [(Any)])])]` | `None` | The value to increase/decrease. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` | Any valid integer(s) or floating point number(s) supported by this operation. |


## Output

| Type | Description |
|------|-------------|
| `[(Any)]` | The result of the operation, usually in the same type as the input value. |


## Examples

```clojure
 10.0 >= .mutable
(Math.Inc
 ;:Value
 .mutable)
.mutable
(Assert.Is 11.0 true)
```


--8<-- "includes/license.md"

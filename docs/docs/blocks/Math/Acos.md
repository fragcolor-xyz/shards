---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Acos

```clojure
(Math.Acos)
```


## Definition

Calculates `Acos()` on the input value and returns its result, or a sequence of results if input is a sequence.


## Input

| Type | Description |
|------|-------------|
| `[(Float) (Float2) (Float3) (Float4) (Seq [(Any)])]` | Any valid floating point number(s) supported by this operation. |


## Output

| Type | Description |
|------|-------------|
| `[(Float) (Float2) (Float3) (Float4) (Seq [(Any)])]` | The result of the operation, usually in the same type as the input value. |


## Examples

```clojure
(Float4 -1.0 0.0 0.5 1.0)
(Math.Acos)
(Assert.Is (Float4 3.1415927 1.5707963 1.0471976 0.0) true)
```

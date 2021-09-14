---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Sqrt

```clojure
(Math.Sqrt)
```


## Definition

Calculates `Sqrt()` on the input value and returns its result, or a sequence of results if input is a sequence.


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
(Float3 1.0 2.0 4.0)
(Math.Sqrt)
(Assert.Is (Float3 1.0 1.4142136 2.0) true)
```

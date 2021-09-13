---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Exp2

```clojure
(Math.Exp2)
```


## Definition

Calculates `Exp2()` on the input value and returns its result, or a sequence of results if input is a sequence.


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
(Float 5)
(Math.Exp2)
(Assert.Is (Float 32) true)
```

---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Atanh

```clojure
(Math.Atanh)
```


## Definition

Calculates `Atanh()` on the input value and returns its result, or a sequence of results if input is a sequence.


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
(Float3 -0.5 0.0 0.5)
(Math.Atanh)
(Assert.Is (Float3 -0.54930614 0.0 0.54930614) true)
```

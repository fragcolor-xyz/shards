---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Asin

```clojure
(Math.Asin)
```


## Definition

Calculates `Asin()` on the input value and returns its result, or a sequence of results if input is a sequence.


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
(Math.Asin)
(Assert.Is (Float4 -1.5707963 0.0 0.52359878 1.5707963) true)
```

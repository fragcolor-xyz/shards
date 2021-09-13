---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Sin

```clojure
(Math.Sin)
```


## Definition

Calculates `Sin()` on the input value and returns its result, or a sequence of results if input is a sequence.


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
(Float3 0.0 1.0 1.5707963)
(Math.Sin)
(Assert.Is (Float3 0.0 0.84147098 1.0) true)
```

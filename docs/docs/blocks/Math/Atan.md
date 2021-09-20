---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Atan

```clojure
(Math.Atan)
```


## Definition

Calculates `Atan()` on the input value and returns its result, or a sequence of results if input is a sequence.


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
(Float3 -1.0 0.0 1.0)
(Math.Atan)
(Assert.Is (Float3 -0.78539816 0.0 0.78539816) true)
```


--8<-- "includes/license.md"

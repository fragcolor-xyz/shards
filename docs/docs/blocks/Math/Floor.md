---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Floor

```clojure
(Math.Floor)
```


## Definition

Calculates `Floor()` on the input value and returns its result, or a sequence of results if input is a sequence.


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
(Float3 -0.95 4.0 7.54)
(Math.Floor)
(Assert.Is (Float3 -1.0 4.0 7.0) true)
```


--8<-- "includes/license.md"

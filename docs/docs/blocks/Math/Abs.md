---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Abs

```clojure
(Math.Abs)
```


## Definition

Calculates `Abs()` on the input value and returns its result, or a sequence of results if input is a sequence.


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
(Const [-0.1 -0.2 0.4])
(Math.Abs)
(Assert.Is [0.1 0.2 0.4] true)
```


--8<-- "includes/license.md"

---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.Abs

```clojure
(BigInt.Abs)
```


## Definition

Computes the absolute value of a big integer.


## Input

| Type | Description |
|------|-------------|
| `[(Bytes)]` | Big integer represented as bytes. |


## Output

| Type | Description |
|------|-------------|
| `[(Bytes)]` | Big integer represented as bytes. |


## Examples

```clojure
1 (BigInt) = .expected

-1 (BigInt)
(BigInt.Abs)
(BigInt.Is .expected) (Assert.Is true true)
```


--8<-- "includes/license.md"

---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.ToInt

```clojure
(BigInt.ToInt)
```


## Definition

Converts a big integer value to an integer.


## Input

| Type | Description |
|------|-------------|
| `[(Bytes)]` | Big integer represented as bytes. |


## Output

| Type | Description |
|------|-------------|
| `[(Int)]` | Integer representation of the big integer value. |


## Examples

```clojure
42 (BigInt)
(BigInt.ToInt)
(Assert.Is (Int 42) true)
```

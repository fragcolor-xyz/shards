---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.ToString

```clojure
(BigInt.ToString)
```


## Definition

Converts the value to a string representation.


## Input

| Type | Description |
|------|-------------|
| `[(Bytes)]` | Big integer represented as bytes. |


## Output

| Type | Description |
|------|-------------|
| `[(String)]` | String representation of the big integer value. |


## Examples

```clojure
42 (BigInt) (BigInt.Shift 20)
(BigInt.ToString)
(Assert.Is "4200000000000000000000" true)
```


--8<-- "includes/license.md"

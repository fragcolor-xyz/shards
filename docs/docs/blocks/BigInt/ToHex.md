---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.ToHex

```clojure
(BigInt.ToHex)
```


## Definition

Converts the value to a hexadecimal representation.


## Input

| Type | Description |
|------|-------------|
| `[(Int) (Bytes) (String)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(String)]` | Hexadecimal representation of the integer value. |


## Examples

```clojure
42 (BigInt)
(BigInt.ToHex)
(Assert.Is "0x2a" true)
```


--8<-- "includes/license.md"

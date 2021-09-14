---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.FromFloat

```clojure
(BigInt.FromFloat
  :ShiftedBy [(Int)]
)
```


## Definition

Converts a floating point number to a big integer.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| ShiftedBy | `[(Int)]` | `0` | The shift is of the decimal point, i.e. of powers of ten, and is to the left if n is negative or to the right if n is positive. |


## Input

| Type | Description |
|------|-------------|
| `[(Float)]` | Floating point number. |


## Output

| Type | Description |
|------|-------------|
| `[(Bytes)]` | Big integer represented as bytes. |


## Examples

```clojure
42 (BigInt) = .expected

42.1 (BigInt.FromFloat)
(BigInt.Is .expected) (Assert.Is true true)
```

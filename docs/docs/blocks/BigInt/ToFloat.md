---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.ToFloat

```clojure
(BigInt.ToFloat
  :ShiftedBy [(Int)]
)
```


## Definition

Converts a big integer value to a floating point number.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| ShiftedBy | `[(Int)]` | `0` | The shift is of the decimal point, i.e. of powers of ten, and is to the left if n is negative or to the right if n is positive. |


## Input

| Type | Description |
|------|-------------|
| `[(Bytes)]` | Big integer represented as bytes. |


## Output

| Type | Description |
|------|-------------|
| `[(Float)]` | Floating point number representation of the big integer value. |


## Examples

```clojure
3 (BigInt)
(BigInt.ToFloat
 ;:ShiftedBy
 1)
(Assert.Is (Float 30.0) true)
```

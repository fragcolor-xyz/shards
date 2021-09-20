---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.Shift

```clojure
(BigInt.Shift
  :By [(Int) (ContextVar [(Int)])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| By | `[(Int) (ContextVar [(Int)])]` | `0` | The shift is of the decimal point, i.e. of powers of ten, and is to the left if n is negative or to the right if n is positive. |


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
4200 (BigInt) = .expected

42 (BigInt)
(BigInt.Shift
 ;:By
 2)
(BigInt.Is .expected) (Assert.Is true true)
```


--8<-- "includes/license.md"

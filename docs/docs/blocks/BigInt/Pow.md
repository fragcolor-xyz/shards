---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.Pow

```clojure
(BigInt.Pow
  :Operand [(Int) (ContextVar [(Int)])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Operand | `[(Int) (ContextVar [(Int)])]` | `None` | The integer operand, can be a variable |


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
8 (BigInt) = .expected

2 (BigInt)
(BigInt.Pow
 ;:Operand
 3)
(BigInt.Is .expected) (Assert.Is true true)
```


--8<-- "includes/license.md"

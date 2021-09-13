---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.Multiply

```clojure
(BigInt.Multiply
  :Operand [(ContextVar [(Bytes)]) (ContextVar [(Seq [(Bytes)])])]
)
```


## Definition

Applies the binary operation on the input value and operand and returns its result, or a sequence of results if input and operand are sequences.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Operand | `[(ContextVar [(Bytes)]) (ContextVar [(Seq [(Bytes)])])]` | `0` | The bytes variable representing the operand |


## Input

| Type | Description |
|------|-------------|
| `[(Bytes) (Seq [(Bytes)])]` | Any valid big integer(s) represented as bytes supported by this operation. |


## Output

| Type | Description |
|------|-------------|
| `[(Bytes) (Seq [(Bytes)])]` | The result of the operation, usually in the same type as the input value. |


## Examples

```clojure
24 (BigInt) = .expected
3 (BigInt) = .operand

8 (BigInt)
(BigInt.Multiply
 ;:Operand
 .operand)
(BigInt.Is .expected) (Assert.Is true true)
```

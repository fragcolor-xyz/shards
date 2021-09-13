---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.Or

```clojure
(BigInt.Or
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
7 (BigInt) = .expected
3 (BigInt) = .operand

6 (BigInt)
(BigInt.Or
 ;:Operand
 .operand)
(BigInt.Is .expected) (Assert.Is true true)
```

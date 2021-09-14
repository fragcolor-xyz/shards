---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.IsLessEqual

```clojure
(BigInt.IsLessEqual
  :Operand [(ContextVar [(Bytes)])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Operand | `[(ContextVar [(Bytes)])]` | `None` | The bytes variable representing the operand |


## Input

| Type | Description |
|------|-------------|
| `[(Bytes)]` | Big integer represented as bytes. |


## Output

| Type | Description |
|------|-------------|
| `[(Bool)]` | A boolean value repesenting the result of the logic operation. |


## Examples

```clojure
11 (BigInt) = .expected

9 (BigInt)
(BigInt.IsLessEqual
 ;:Operand
 .expected)
(Assert.Is true true)
```

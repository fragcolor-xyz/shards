---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# BigInt.ToBytes

```clojure
(BigInt.ToBytes
  :Bits [(Int)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Bits | `[(Int)]` | `0` | The desired amount of bits for the output or 0 for automatic packing. |


## Input

| Type | Description |
|------|-------------|
| `[(Bytes)]` | Big integer represented as bytes. |


## Output

| Type | Description |
|------|-------------|
| `[(Bytes)]` |  |


## Examples

```clojure
42 (BigInt)
(BigInt.ToBytes
 ;:Bits
 16)
(ToHex)
(Assert.Is "0x002a" true)
```

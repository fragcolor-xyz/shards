---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Expand

```clojure
(Expand
  :Size [(Int)]
  :Chain [(Chain) (String) (None) (ContextVar [(Chain)])]
  :Policy [(Enum)]
  :Threads [(Int)]
  :Coroutines [(Int)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Size | `[(Int)]` | `10` | The maximum expansion size. |
| Chain | `[(Chain) (String) (None) (ContextVar [(Chain)])]` | `None` | The chain to spawn and try to run many times concurrently. |
| Policy | `[(Enum)]` | `Enum: 1 vendor: 0x73696e6b type: 0x7472794d` | The execution policy in terms of chains success. |
| Threads | `[(Int)]` | `1` | The number of cpu threads to use. |
| Coroutines | `[(Int)]` | `1` | The number of coroutines to run on each thread. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Seq [(Any)])]` |  |


## Examples

```clojure
21
(Expand
 :Size 10
 :Chain (defchain wide-test (Math.Multiply 2))
 :Threads 5)
(Assert.Is [42 42 42 42 42 42 42 42 42 42] true)
```

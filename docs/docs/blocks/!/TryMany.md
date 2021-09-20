---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# TryMany

```clojure
(TryMany
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
| Chain | `[(Chain) (String) (None) (ContextVar [(Chain)])]` | `None` | The chain to spawn and try to run many times concurrently. |
| Policy | `[(Enum)]` | `Enum: 1 vendor: 0x73696e6b type: 0x7472794d` | The execution policy in terms of chains success. |
| Threads | `[(Int)]` | `1` | The number of cpu threads to use. |
| Coroutines | `[(Int)]` | `1` | The number of coroutines to run on each thread. |


## Input

| Type | Description |
|------|-------------|
| `[(Seq [(Any)])]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Seq [(Any)])]` |  |


## Examples

```clojure
(Const ["A" "B" "C"])
(TryMany
 ;:Chain
 (Chain
  "print"
  (Log)
  "Ok")
 ;:Policy
 WaitUntil.AllSuccess
 ;:Threads
 12
 ;:Coroutine
 1)
(Assert.Is ["Ok" "Ok" "Ok"] true)
```


--8<-- "includes/license.md"

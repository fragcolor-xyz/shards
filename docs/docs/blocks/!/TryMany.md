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
| Chain | `[(Chain) (String) (None) (ContextVar [(Chain)])]` |  | The chain to spawn and try to run many times concurrently. |
| Policy | `[(Enum)]` |  | The execution policy in terms of chains success. |
| Threads | `[(Int)]` |  | The number of cpu threads to use. |
| Coroutines | `[(Int)]` |  | The number of coroutines to run on each thread. |


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
(TryMany

)
```

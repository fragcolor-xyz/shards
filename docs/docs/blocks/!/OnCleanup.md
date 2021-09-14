---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# OnCleanup

```clojure
(OnCleanup
  :Blocks [(Block) (Seq [(Block)]) (None)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Blocks | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks to execute on chain's cleanup. Notice that blocks that suspend execution are not allowed. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Examples

```clojure
(OnCleanup

)
```

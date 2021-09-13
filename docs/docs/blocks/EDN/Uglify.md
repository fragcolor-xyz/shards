---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# EDN.Uglify

```clojure
(EDN.Uglify
  :Hooks [(Seq [(Any)])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Hooks | `[(Seq [(Any)])]` | `None` | A list of pairs to hook, [<symbol name> <blocks to execute>], blocks will have as input the contents of the symbols's list. |


## Input

| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Examples

```clojure
(EDN.Uglify

)
```

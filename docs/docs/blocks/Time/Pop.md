---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Time.Pop

```clojure
(Time.Pop
  :Sequence [(ContextVar [(Any) (Int)]) (None)]
)
```


## Definition

This blocks delays its output until one of the values of the sequence parameter expires.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Sequence | `[(ContextVar [(Any) (Int)]) (None)]` | `None` | A variables sequence of pairs [value pop-epoch-time-ms] with types [Any Int] |


## Input

| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Examples

```clojure
(Time.Pop

)
```


--8<-- "includes/license.md"

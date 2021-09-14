---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Spawn

```clojure
(Spawn
  :Chain [(Chain) (String) (None) (ContextVar [(Chain)])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chain | `[(Chain) (String) (None) (ContextVar [(Chain)])]` | `None` | The chain to spawn and try to run many times concurrently. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Chain)]` |  |


## Examples

```clojure
(defchain logic
  (IsMore 10) (Or) (IsLess 0))

11
~[(Spawn logic) >= .ccc
  (Wait .ccc)]
(Assert.Is true true)
```

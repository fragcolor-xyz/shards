---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Pause

```clojure
(Pause
  :Time [(None) (Float) (Int) (ContextVar [(Float)]) (ContextVar [(Int)])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Time | `[(None) (Float) (Int) (ContextVar [(Float)]) (ContextVar [(Int)])]` | `None` | The amount of time in seconds (can be fractional) to pause this chain. |


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
(Pause

)
```


--8<-- "includes/license.md"

---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Slice

```clojure
(Slice
  :From [(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])])]
  :To [(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])]) (None)]
  :Step [(Int)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| From | `[(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])])]` | `0` | From index. |
| To | `[(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])]) (None)]` | `None` | To index (excluding). |
| Step | `[(Int)]` | `1` | The increment between each index. |


## Input

| Type | Description |
|------|-------------|
| `[(Seq [(Any)]) (Bytes) (String)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Examples

```clojure
(Slice

)
```


--8<-- "includes/license.md"

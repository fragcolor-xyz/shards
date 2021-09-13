---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# GUI.TreeNode

```clojure
(GUI.TreeNode
  :Label [(String)]
  :Contents [(Block) (Seq [(Block)]) (None)]
  :StartOpen [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Label | `[(String)]` | `""` | The label of this node. |
| Contents | `[(Block) (Seq [(Block)]) (None)]` | `None` | The contents of this node. |
| StartOpen | `[(Bool)]` | `true` | If this node should start in the open state. |


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
(GUI.TreeNode

)
```

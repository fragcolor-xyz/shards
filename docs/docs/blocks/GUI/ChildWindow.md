---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# GUI.ChildWindow

```clojure
(GUI.ChildWindow
  :Width [(Int) (None)]
  :Height [(Int) (None)]
  :Border [(Bool)]
  :Contents [(Block) (Seq [(Block)]) (None)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Width | `[(Int) (None)]` | `None` | The width of the child window to create |
| Height | `[(Int) (None)]` | `None` | The height of the child window to create. |
| Border | `[(Bool)]` | `false` | If we want to draw a border frame around the child window. |
| Contents | `[(Block) (Seq [(Block)]) (None)]` | `None` | The inner contents blocks. |


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
(GUI.ChildWindow

)
```

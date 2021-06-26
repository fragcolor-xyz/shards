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
| Width | `[(Int) (None)]` |  | The width of the child window to create |
| Height | `[(Int) (None)]` |  | The height of the child window to create. |
| Border | `[(Bool)]` |  | If we want to draw a border frame around the child window. |
| Contents | `[(Block) (Seq [(Block)]) (None)]` |  | The inner contents blocks. |


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

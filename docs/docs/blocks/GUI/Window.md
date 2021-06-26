# GUI.Window

```clojure
(GUI.Window
  :Title [(String)]
  :Pos [(Int2) (Float2) (None)]
  :Width [(Int) (Float)]
  :Height [(Int) (Float)]
  :Contents [(Block) (Seq [(Block)]) (None)]
  :AllowMove [(Bool)]
  :AllowResize [(Bool)]
  :AllowCollapse [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Title | `[(String)]` |  | The title of the window to create. |
| Pos | `[(Int2) (Float2) (None)]` |  | The x/y position of the window to create. If the value is a Float2, it will be interpreted as relative to the container window size. |
| Width | `[(Int) (Float)]` |  | The width of the window to create. If the value is a Float, it will be interpreted as relative to the container window size. |
| Height | `[(Int) (Float)]` |  | The height of the window to create. If the value is a Float, it will be interpreted as relative to the container window size. |
| Contents | `[(Block) (Seq [(Block)]) (None)]` |  | The inner contents blocks. |
| AllowMove | `[(Bool)]` |  | If the window can be moved. |
| AllowResize | `[(Bool)]` |  | If the window can be resized. |
| AllowCollapse | `[(Bool)]` |  | If the window can be collapsed. |


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
(GUI.Window

)
```

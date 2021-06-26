# GFX.MainWindow

```clojure
(GFX.MainWindow
  :Title [(String)]
  :Width [(Int)]
  :Height [(Int)]
  :Contents [(Block) (Seq [(Block)]) (None)]
  :Fullscreen [(Bool)]
  :Debug [(Bool)]
  :ClearColor [(Color)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Title | `[(String)]` |  | The title of the window to create. |
| Width | `[(Int)]` |  | The width of the window to create. In pixels and DPI aware. |
| Height | `[(Int)]` |  | The height of the window to create. In pixels and DPI aware. |
| Contents | `[(Block) (Seq [(Block)]) (None)]` |  | The contents of this window. |
| Fullscreen | `[(Bool)]` |  | If the window should use fullscreen mode. |
| Debug | `[(Bool)]` |  | If the device backing the window should be created with debug layers on. |
| ClearColor | `[(Color)]` |  | The color to use to clear the backbuffer at the beginning of a new frame. |


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
(GFX.MainWindow

)
```

# GUI.Plot

```clojure
(GUI.Plot
  :Title [(String)]
  :Contents [(Block) (Seq [(Block)]) (None)]
  :X_Label [(String)]
  :Y_Label [(String)]
  :X_Limits [(None) (Float2) (ContextVar [(Float2)])]
  :Y_Limits [(None) (Float2) (ContextVar [(Float2)])]
  :Lock_X [(Bool) (ContextVar [(Bool)])]
  :Lock_Y [(Bool) (ContextVar [(Bool)])]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Title | `[(String)]` | `""` | The title of the plot to create. |
| Contents | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks describing this plot. |
| X_Label | `[(String)]` | `""` | The X axis label. |
| Y_Label | `[(String)]` | `""` | The Y axis label. |
| X_Limits | `[(None) (Float2) (ContextVar [(Float2)])]` | `None` | The X axis limits. |
| Y_Limits | `[(None) (Float2) (ContextVar [(Float2)])]` | `None` | The Y axis limits. |
| Lock_X | `[(Bool) (ContextVar [(Bool)])]` | `false` | If the X axis should be locked into its limits. |
| Lock_Y | `[(Bool) (ContextVar [(Bool)])]` | `false` | If the Y axis should be locked into its limits. |


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
(GUI.Plot

)
```

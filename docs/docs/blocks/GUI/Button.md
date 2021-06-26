# GUI.Button

```clojure
(GUI.Button
  :Label [(String)]
  :Action [(Block) (Seq [(Block)]) (None)]
  :Type [(Enum)]
  :Size [(Float2)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Label | `[(String)]` |  | The text label of this button. |
| Action | `[(Block) (Seq [(Block)]) (None)]` |  | The blocks to execute when the button is pressed. |
| Type | `[(Enum)]` |  | The button type. |
| Size | `[(Float2)]` |  | The optional size override. |


## Input
| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Bool)]` |  |


## Examples

```clojure
(GUI.Button

)
```

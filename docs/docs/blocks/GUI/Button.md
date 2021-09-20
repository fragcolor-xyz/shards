---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


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
| Label | `[(String)]` | `""` | The text label of this button. |
| Action | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks to execute when the button is pressed. |
| Type | `[(Enum)]` | `Enum: 0 vendor: 0x73696e6b type: 0x67756942` | The button type. |
| Size | `[(Float2)]` | `(0, 0)` | The optional size override. |


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


--8<-- "includes/license.md"

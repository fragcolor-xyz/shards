---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# GUI.Float3Drag

```clojure
(GUI.Float3Drag
  :Label [(String) (None)]
  :Variable [(String) (None)]
  :Speed [(String) (None)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Label | `[(String) (None)]` | `None` | The label for this widget. |
| Variable | `[(String) (None)]` | `None` | The name of the variable that holds the input value. |
| Speed | `[(String) (None)]` | `1` | The speed multiplier for this drag widget. |


## Input

| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Float3)]` |  |


## Examples

```clojure
(GUI.Float3Drag

)
```


--8<-- "includes/license.md"

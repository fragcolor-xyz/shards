---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# GUI.PlotBars

```clojure
(GUI.PlotBars
  :Label [(String)]
  :Color [(None) (Color)]
  :Width [(Float)]
  :Horizontal [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Label | `[(String)]` | `""` | The plot's label. |
| Color | `[(None) (Color)]` | `None` | The plot's color. |
| Width | `[(Float)]` | `None` | The width of each bar |
| Horizontal | `[(Bool)]` | `0.67` | If the bar should be horiziontal rather than vertical |


## Input

| Type | Description |
|------|-------------|
| `[(Seq [(Float)]) (Seq [(Float2)])]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Seq [(Float)]) (Seq [(Float2)])]` |  |


## Examples

```clojure
(GUI.PlotBars

)
```

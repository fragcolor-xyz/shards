---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# XR.Render

```clojure
(XR.Render
  :Contents [(Block) (Seq [(Block)]) (None)]
  :Near [(Float)]
  :Far [(Float)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Contents | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks expressing the contents to render. |
| Near | `[(Float)]` | `0.1` | The distance from the near clipping plane. |
| Far | `[(Float)]` | `1000` | The distance from the far clipping plane. |


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
(XR.Render

)
```


--8<-- "includes/license.md"

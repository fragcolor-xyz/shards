---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# GFX.RenderTexture

```clojure
(GFX.RenderTexture
  :Width [(Int)]
  :Height [(Int)]
  :Contents [(Block) (Seq [(Block)]) (None)]
  :GUI [(Bool)]
  :ClearColor [(Color)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Width | `[(Int)]` | `256` | The width of the texture to render. |
| Height | `[(Int)]` | `256` | The height of the texture to render. |
| Contents | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks expressing the contents to render. |
| GUI | `[(Bool)]` | `false` | If this render target should be able to render GUI blocks within. If false any GUI block inside will be rendered on the top level surface. |
| ClearColor | `[(Color)]` | `48, 48, 48, 255` | The color to use to clear the backbuffer at the beginning of a new frame. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Object)]` |  |


## Examples

```clojure
(GFX.RenderTexture

)
```

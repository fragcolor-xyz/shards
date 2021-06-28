# GFX.Model

```clojure
(GFX.Model
  :Layout [(Seq [(Enum)])]
  :Dynamic [(Bool)]
  :CullMode [(Enum)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Layout | `[(Seq [(Enum)])]` | `[]` | The vertex layout of this model. |
| Dynamic | `[(Bool)]` | `false` | If the model is dynamic and will be optimized to change as often as every frame. |
| CullMode | `[(Enum)]` | `Enum: 2 vendor: 0x73696e6b type: 0x67667843` | Triangles facing the specified direction are not drawn. |


## Input
| Type | Description |
|------|-------------|
| `[(Table {"Vertices" (Seq [(Float) (Float2) (Float3) (Color) (Int)]) "Indices" (Seq [(Int3)])})]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Object)]` |  |


## Examples

```clojure
(GFX.Model

)
```

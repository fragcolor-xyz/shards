# GFX.Draw

```clojure
(GFX.Draw
  :Shader [(Object) (ContextVar [(Object)])]
  :Textures [(Object) (ContextVar [(Object)]) (Seq [(Object)]) (ContextVar [(Seq [(Object)])]) (None)]
  :Model [(Object) (ContextVar [(Object)]) (None)]
  :Blend [(None) (Table {"Src" (Enum) "Dst" (Enum) "Op" (Enum)}) (Seq [(Table {"Src" (Enum) "Dst" (Enum) "Op" (Enum)})])]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Shader | `[(Object) (ContextVar [(Object)])]` |  | The shader program to use for this draw. |
| Textures | `[(Object) (ContextVar [(Object)]) (Seq [(Object)]) (ContextVar [(Seq [(Object)])]) (None)]` |  | A texture or the textures to pass to the shaders. |
| Model | `[(Object) (ContextVar [(Object)]) (None)]` |  | The model to draw. If no model is specified a full screen quad will be used. |
| Blend | `[(None) (Table {"Src" (Enum) "Dst" (Enum) "Op" (Enum)}) (Seq [(Table {"Src" (Enum) "Dst" (Enum) "Op" (Enum)})])]` |  | The blend state table to describe and enable blending. If it's a single table the state will be assigned to both RGB and Alpha, if 2 tables are specified, the first will be RGB, the second Alpha. |


## Input
| Type | Description |
|------|-------------|
| `[(Seq [(Float4)]) (Seq [(Seq [(Float4)])])]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Seq [(Float4)]) (Seq [(Seq [(Float4)])])]` |  |


## Examples

```clojure
(GFX.Draw

)
```

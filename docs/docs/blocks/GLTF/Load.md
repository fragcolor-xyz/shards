# GLTF.Load

```clojure
(GLTF.Load
  :Textures [(Bool)]
  :Shaders [(Bool)]
  :TransformBefore [(Seq [(Float4)]) (None)]
  :TransformAfter [(Seq [(Float4)]) (None)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Textures | `[(Bool)]` | `true` | If the textures linked to this model should be loaded. |
| Shaders | `[(Bool)]` | `true` | If the shaders linked to this model should be compiled and/or loaded. |
| TransformBefore | `[(Seq [(Float4)]) (None)]` | `None` | An optional global transformation matrix to apply before the model root nodes transformations. |
| TransformAfter | `[(Seq [(Float4)]) (None)]` | `None` | An optional global transformation matrix to apply after the model root nodes transformations. |


## Input
| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Object)]` |  |


## Examples

```clojure
(GLTF.Load

)
```

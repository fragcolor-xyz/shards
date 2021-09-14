---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# GLTF.Load

```clojure
(GLTF.Load
  :Textures [(Bool)]
  :Shaders [(Bool)]
  :TransformBefore [(Seq [(Float4)]) (None)]
  :TransformAfter [(Seq [(Float4)]) (None)]
  :Animations [(Seq [(String)]) (None)]
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
| Animations | `[(Seq [(String)]) (None)]` | `None` | A list of animations to load and prepare from the gltf file, if not found an error is raised. |


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

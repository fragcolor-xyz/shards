# GFX.Camera

```clojure
(GFX.Camera
  :Near [(Float)]
  :Far [(Float)]
  :FieldOfView [(Float)]
  :OffsetX [(Int)]
  :OffsetY [(Int)]
  :Width [(Int)]
  :Height [(Int)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Near | `[(Float)]` |  | The distance from the near clipping plane. |
| Far | `[(Float)]` |  | The distance from the far clipping plane. |
| FieldOfView | `[(Float)]` |  | The field of view of the camera. |
| OffsetX | `[(Int)]` |  | The horizontal offset of the viewport. |
| OffsetY | `[(Int)]` |  | The vertical offset of the viewport. |
| Width | `[(Int)]` |  | The width of the viewport, if 0 it will use the full current view width. |
| Height | `[(Int)]` |  | The height of the viewport, if 0 it will use the full current view height. |


## Input
| Type | Description |
|------|-------------|
| `[(None) (Table {"Position" (Float3) "Target" (Float3)})]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(None) (Table {"Position" (Float3) "Target" (Float3)})]` |  |


## Examples

```clojure
(GFX.Camera

)
```

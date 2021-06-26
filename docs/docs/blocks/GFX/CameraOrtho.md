# GFX.CameraOrtho

```clojure
(GFX.CameraOrtho
  :Left [(Float)]
  :Right [(Float)]
  :Bottom [(Float)]
  :Top [(Float)]
  :Near [(Float)]
  :Far [(Float)]
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
| Left | `[(Float)]` |  | The left of the projection. |
| Right | `[(Float)]` |  | The right of the projection. |
| Bottom | `[(Float)]` |  | The bottom of the projection. |
| Top | `[(Float)]` |  | The top of the projection. |
| Near | `[(Float)]` |  | The distance from the near clipping plane. |
| Far | `[(Float)]` |  | The distance from the far clipping plane. |
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
(GFX.CameraOrtho

)
```

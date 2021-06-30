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
| Left | `[(Float)]` | `0` | The left of the projection. |
| Right | `[(Float)]` | `1` | The right of the projection. |
| Bottom | `[(Float)]` | `1` | The bottom of the projection. |
| Top | `[(Float)]` | `0` | The top of the projection. |
| Near | `[(Float)]` | `0` | The distance from the near clipping plane. |
| Far | `[(Float)]` | `100` | The distance from the far clipping plane. |
| OffsetX | `[(Int)]` | `0` | The horizontal offset of the viewport. |
| OffsetY | `[(Int)]` | `0` | The vertical offset of the viewport. |
| Width | `[(Int)]` | `0` | The width of the viewport, if 0 it will use the full current view width. |
| Height | `[(Int)]` | `0` | The height of the viewport, if 0 it will use the full current view height. |


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

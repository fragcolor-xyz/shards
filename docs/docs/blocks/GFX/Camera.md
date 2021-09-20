---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


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
| Near | `[(Float)]` | `0.1` | The distance from the near clipping plane. |
| Far | `[(Float)]` | `1000` | The distance from the far clipping plane. |
| FieldOfView | `[(Float)]` | `60` | The field of view of the camera. |
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
(GFX.Camera

)
```


--8<-- "includes/license.md"

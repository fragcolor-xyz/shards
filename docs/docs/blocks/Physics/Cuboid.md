# Physics.Cuboid

```clojure
(Physics.Cuboid
  :Position [(Float3) (ContextVar [(Float3)])]
  :Rotation [(Float4) (ContextVar [(Float4)])]
  :HalfExtents [(Float3)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Position | `[(Float3) (ContextVar [(Float3)])]` | `(0, 0, 0)` | The position wrt. the body it is attached to. |
| Rotation | `[(Float4) (ContextVar [(Float4)])]` | `(0, 0, 0, 1)` | The rotation  wrt. the body it is attached to |
| HalfExtents | `[(Float3)]` | `(0.5, 0.5, 0.5)` | The half-extents of the cuboid shape. |


## Input
| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Object)]` |  |


## Examples

```clojure
(Physics.Cuboid

)
```

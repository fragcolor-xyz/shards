# GFX.Unproject

```clojure
(GFX.Unproject
  :Z [(Float)]
)
```

## Definition
This block unprojects screen coordinates into world coordinates.

## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Z | `[(Float)]` |  | The value of Z depth to use, generally 0.0 for  the near plane, 1.0 for the far plane. |


## Input
| Type | Description |
|------|-------------|
| `[(Float2)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Float3)]` |  |


## Examples

```clojure
(GFX.Unproject

)
```

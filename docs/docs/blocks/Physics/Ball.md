# Physics.Ball

```clojure
(Physics.Ball
  :Position [(Float3) (ContextVar [(Float3)])]
  :Rotation [(Float4) (ContextVar [(Float4)])]
  :Radius [(Float)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Position | `[(Float3) (ContextVar [(Float3)])]` | `(0, 0, 0)` | The position wrt. the body it is attached to. |
| Rotation | `[(Float4) (ContextVar [(Float4)])]` | `(0, 0, 0, 1)` | The rotation  wrt. the body it is attached to |
| Radius | `[(Float)]` | `0.5` | The radius of the sphere. |


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
(Physics.Ball

)
```

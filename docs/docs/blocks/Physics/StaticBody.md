# Physics.StaticBody

```clojure
(Physics.StaticBody
  :Shapes [(ContextVar [(Object)]) (ContextVar [(Object)]) (None)]
  :Position [(Float3) (ContextVar [(Float3)]) (Seq [(Float3)]) (ContextVar [(Seq [(Float3)])])]
  :Rotation [(Float4) (ContextVar [(Float4)]) (Seq [(Float4)]) (ContextVar [(Seq [(Float4)])])]
  :Name [(String) (None)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Shapes | `[(ContextVar [(Object)]) (ContextVar [(Object)]) (None)]` | `None` | The shape or shapes of this rigid body. |
| Position | `[(Float3) (ContextVar [(Float3)]) (Seq [(Float3)]) (ContextVar [(Seq [(Float3)])])]` | `(0, 0, 0)` | The initial position of this rigid body. Can be updated in the case of a kinematic rigid body. |
| Rotation | `[(Float4) (ContextVar [(Float4)]) (Seq [(Float4)]) (ContextVar [(Seq [(Float4)])])]` | `(0, 0, 0, 1)` | The initial rotation of this rigid body. Either axis angles in radians Float3 or a quaternion Float4. Can be updated in the case of a kinematic rigid body. |
| Name | `[(String) (None)]` | `None` | The optional name of the variable that will be exposed to identify, apply forces (if dynamic) and control this rigid body. |


## Input
| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Examples

```clojure
(Physics.StaticBody

)
```

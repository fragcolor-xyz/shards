# XR.Controller

```clojure
(XR.Controller
  :Hand [(Enum)]
  :Inverse [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Hand | `[(Enum)]` |  | Which hand we want to track. |
| Inverse | `[(Bool)]` |  | If the output should be the inverse transformation matrix. |


## Input
| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Table {"handedness" (Enum) "transform" (Seq [(Float4)]) "inverseTransform" (Seq [(Float4)]) "buttons" (Seq [(Float3)]) "sticks" (Seq [(Float2)]) "id" (String) "connected" (Bool)})]` |  |


## Examples

```clojure
(XR.Controller

)
```

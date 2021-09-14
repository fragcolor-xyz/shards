---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


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
| Hand | `[(Enum)]` | `Enum: 0 vendor: 0x73696e6b type: 0x78726861` | Which hand we want to track. |
| Inverse | `[(Bool)]` | `false` | If the output should be the inverse transformation matrix. |


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

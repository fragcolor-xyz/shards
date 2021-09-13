---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Normalize

```clojure
(Math.Normalize
  :Positive [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Positive | `[(Bool)]` | `false` | If the output should be in the range 0.0~1.0 instead of -1.0~1.0. |


## Input

| Type | Description |
|------|-------------|
| `[(Seq [(Float)]) (Float2) (Seq [(Float2)]) (Float3) (Seq [(Float3)]) (Float4) (Seq [(Float4)])]` | Any valid floating point number(s) supported by this operation. |


## Output

| Type | Description |
|------|-------------|
| `[(Seq [(Float)]) (Float2) (Seq [(Float2)]) (Float3) (Seq [(Float3)]) (Float4) (Seq [(Float4)])]` | The result of the operation, usually in the same type as the input value. |


## Examples

```clojure
(Math.Normalize

)
```

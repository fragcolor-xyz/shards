---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Orthographic

```clojure
(Math.Orthographic
  :Width [(Int) (Float)]
  :Height [(Int) (Float)]
  :Near [(Int) (Float)]
  :Far [(Int) (Float)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Width | `[(Int) (Float)]` | `1280` | Width size. |
| Height | `[(Int) (Float)]` | `720` | Height size. |
| Near | `[(Int) (Float)]` | `0` | Near plane. |
| Far | `[(Int) (Float)]` | `1000` | Far plane. |


## Input

| Type | Description |
|------|-------------|
| `[(None)]` | Any valid floating point number(s) supported by this operation. |


## Output

| Type | Description |
|------|-------------|
| `[(Seq [(Float4)])]` | The result of the operation, usually in the same type as the input value. |


## Examples

```clojure
(Math.Orthographic

)
```

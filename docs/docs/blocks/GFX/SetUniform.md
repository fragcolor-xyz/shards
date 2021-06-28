# GFX.SetUniform

```clojure
(GFX.SetUniform
  :Name [(String)]
  :MaxSize [(Int)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Name | `[(String)]` | `""` | The name of the uniform shader variable. Uniforms are so named because they do not change from one shader invocation to the next within a particular rendering call thus their value is uniform among all invocations. Uniform names are unique. |
| MaxSize | `[(Int)]` | `1` | If the input contains multiple values, the maximum expected size of the input. |


## Input
| Type | Description |
|------|-------------|
| `[(Float4) (Seq [(Float4)]) (Seq [(Float3)]) (Seq [(Float4)]) (Seq [(Seq [(Float4)])]) (Seq [(Seq [(Float3)])])]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Float4) (Seq [(Float4)]) (Seq [(Float3)]) (Seq [(Float4)]) (Seq [(Seq [(Float4)])]) (Seq [(Seq [(Float3)])])]` |  |


## Examples

```clojure
(GFX.SetUniform

)
```

# FloatsToImage

```clojure
(FloatsToImage
  :Width [(Int)]
  :Height [(Int)]
  :Channels [(Int)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Width | `[(Int)]` | `16` | The width of the output image. |
| Height | `[(Int)]` | `16` | The height of the output image. |
| Channels | `[(Int)]` | `1` | The channels of the output image. |


## Input
| Type | Description |
|------|-------------|
| `[(Seq [(Float)])]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Image)]` |  |


## Examples

```clojure
(FloatsToImage

)
```

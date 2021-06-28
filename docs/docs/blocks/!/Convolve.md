# Convolve

```clojure
(Convolve
  :Radius [(Int)]
  :Step [(Int)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Radius | `[(Int)]` | `1` | The radius of the kernel, e.g. 1 = 1x1; 2 = 3x3; 3 = 5x5 and so on. |
| Step | `[(Int)]` | `1` | How many pixels to advance each activation. |


## Input
| Type | Description |
|------|-------------|
| `[(Image)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Image)]` |  |


## Examples

```clojure
(Convolve

)
```

# DSP.IFFT

```clojure
(DSP.IFFT
  :Audio [(Bool)]
  :Complex [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Audio | `[(Bool)]` |  | If the output should be an Audio chunk. |
| Complex | `[(Bool)]` |  | If the output should be complex numbers (only if not Audio). |


## Input
| Type | Description |
|------|-------------|
| `[(Seq [(Float2)])]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Seq [(Float)]) (Seq [(Float2)]) (Audio)]` |  |


## Examples

```clojure
(DSP.IFFT

)
```

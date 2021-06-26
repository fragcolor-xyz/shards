# ExpectLike

```clojure
(ExpectLike
  :Example [(Any)]
  :Unsafe [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Example | `[(Any)]` |  | The example value to expect, in the case of a used chain, the output type of that chain will be used. |
| Unsafe | `[(Bool)]` |  | If we should skip performing deep type hashing and comparison. (generally fast but this might improve performance) |


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
(ExpectLike

)
```

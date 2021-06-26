# LoadImage

```clojure
(LoadImage
  :File [(String) (ContextVar [(String)]) (None)]
  :BPP [(Enum)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| File | `[(String) (ContextVar [(String)]) (None)]` |  | The file to read/write from. |
| BPP | `[(Enum)]` |  | bits per pixel (HDR images loading and such!) |


## Input
| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Image)]` |  |


## Examples

```clojure
(LoadImage

)
```

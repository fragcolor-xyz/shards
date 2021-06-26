# WriteFile

```clojure
(WriteFile
  :File [(String) (ContextVar [(String)]) (None)]
  :Append [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| File | `[(String) (ContextVar [(String)]) (None)]` |  | The file to read/write from. |
| Append | `[(Bool)]` |  | If we should append to the file if existed already or truncate. (default: false). |


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
(WriteFile

)
```

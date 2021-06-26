# FS.Copy

```clojure
(FS.Copy
  :Destination [(String) (ContextVar [(String)]) (None)]
  :Behavior [(Enum)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Destination | `[(String) (ContextVar [(String)]) (None)]` |  | The destination path, can be a file or a directory. |
| Behavior | `[(Enum)]` |  | What to do when the destination already exists. |


## Input
| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Examples

```clojure
(FS.Copy

)
```

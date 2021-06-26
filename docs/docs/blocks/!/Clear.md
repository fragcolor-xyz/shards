# Clear

```clojure
(Clear
  :Name [(String) (ContextVar [(Any)])]
  :Key [(String) (ContextVar [(String)]) (None)]
  :Global [(Bool)]
  :Serialized [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Name | `[(String) (ContextVar [(Any)])]` |  | The name of the variable. |
| Key | `[(String) (ContextVar [(String)]) (None)]` |  | The key of the value to read/write from/in the table (this variable will become a table). |
| Global | `[(Bool)]` |  | If the variable is or should be available to all of the chains in the same node. |
| Serialized | `[(Bool)]` |  | If the variable should be serialized as chain variable when the parent chain is serialized, e.g. (ToBytes), this option is mostly to be used with (Once) blocks with serialization turned off or (Get) blocks with default values set. |


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
(Clear

)
```

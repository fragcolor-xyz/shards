# Get

```clojure
(Get
  :Name [(String) (ContextVar [(Any)])]
  :Key [(String) (ContextVar [(String)]) (None)]
  :Global [(Bool)]
  :Serialized [(Bool)]
  :Default [(Any)]
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
| Default | `[(Any)]` |  | The default value to use to infer types and output if the variable is not set, key is not there and/or type mismatches. |


## Input
| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Examples

```clojure
(Get

)
```

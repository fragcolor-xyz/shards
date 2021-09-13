---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Table

```clojure
(Table
  :Name [(String) (ContextVar [(Any)])]
  :Key [(String) (ContextVar [(String)]) (None)]
  :Global [(Bool)]
  :Serialized [(Bool)]
  :Types [(Enum) (Seq [(Enum)]) (Seq [(Enum) (Seq [(Enum)]) (Self)])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Name | `[(String) (ContextVar [(Any)])]` | `""` | The name of the variable. |
| Key | `[(String) (ContextVar [(String)]) (None)]` | `None` | The key of the value to read/write from/in the table (this variable will become a table). |
| Global | `[(Bool)]` | `false` | If the variable is or should be available to all of the chains in the same node. |
| Serialized | `[(Bool)]` | `false` | If the variable should be serialized as chain variable when the parent chain is serialized, e.g. (ToBytes), this option is mostly to be used with (Once) blocks with serialization turned off or (Get) blocks with default values set. |
| Types | `[(Enum) (Seq [(Enum)]) (Seq [(Enum) (Seq [(Enum)]) (Self)])]` | `Enum: 0 vendor: 0x73696e6b type: 0x74797065` | The sequence inner types to forward declare. |


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
(Table

)
```

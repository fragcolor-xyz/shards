# Erase

```clojure
(Erase
  :Indices [(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])]) (String) (Seq [(String)]) (ContextVar [(String)]) (ContextVar [(Seq [(String)])])]
  :Name [(String) (ContextVar [(Any)])]
  :Key [(String)]
  :Global [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Indices | `[(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])]) (String) (Seq [(String)]) (ContextVar [(String)]) (ContextVar [(Seq [(String)])])]` | `None` | One or multiple indices to filter from a sequence. |
| Name | `[(String) (ContextVar [(Any)])]` | `""` | The name of the variable. |
| Key | `[(String)]` | `None` | The key of the value to read/write from/in the table (this variable will become a table). |
| Global | `[(Bool)]` | `false` | If the variable is or should be available to all of the chains in the same node. |


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
(Erase

)
```

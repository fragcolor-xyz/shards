# Take

```clojure
(Take
  :Indices [(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])]) (String) (Seq [(String)]) (ContextVar [(String)]) (ContextVar [(Seq [(String)])])]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Indices | `[(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])]) (String) (Seq [(String)]) (ContextVar [(String)]) (ContextVar [(Seq [(String)])])]` | `None` | One or multiple indices/keys to extract from a sequence/table. |


## Input
| Type | Description |
|------|-------------|
| `[(Int2) (Int3) (Int4) (Int8) (Int16) (Float2) (Float3) (Float4) (Bytes) (Color) (String) (Seq [(Any)]) (Table [(Any)])]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Examples

```clojure
(Take

)
```

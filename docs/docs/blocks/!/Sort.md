# Sort

```clojure
(Sort
  :From [(ContextVar [(Seq [(Any)])])]
  :Join [(ContextVar [(Seq [(Any)])]) (Seq [(ContextVar [(Seq [(Any)])])])]
  :Desc [(Bool)]
  :Key [(Block) (Seq [(Block)]) (None)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| From | `[(ContextVar [(Seq [(Any)])])]` |  | The name of the sequence variable to edit in place. |
| Join | `[(ContextVar [(Seq [(Any)])]) (Seq [(ContextVar [(Seq [(Any)])])])]` |  | Other columns to join sort/filter using the input (they must be of the same length). |
| Desc | `[(Bool)]` |  | If sorting should be in descending order, defaults ascending. |
| Key | `[(Block) (Seq [(Block)]) (None)]` |  | The blocks to use to transform the collection's items before they are compared. Can be None. |


## Input
| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Seq [(Any)])]` |  |


## Examples

```clojure
(Sort

)
```

# Remove

```clojure
(Remove
  :From [(ContextVar [(Seq [(Any)])])]
  :Join [(ContextVar [(Seq [(Any)])]) (Seq [(ContextVar [(Seq [(Any)])])])]
  :Predicate [(Block) (Seq [(Block)])]
  :Unordered [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| From | `[(ContextVar [(Seq [(Any)])])]` | `None` | The name of the sequence variable to edit in place. |
| Join | `[(ContextVar [(Seq [(Any)])]) (Seq [(ContextVar [(Seq [(Any)])])])]` | `None` | Other columns to join sort/filter using the input (they must be of the same length). |
| Predicate | `[(Block) (Seq [(Block)])]` | `None` | The blocks to use as predicate, if true the item will be popped from the sequence. |
| Unordered | `[(Bool)]` | `false` | Turn on to remove items very quickly but will not preserve the sequence items order. |


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
(Remove

)
```

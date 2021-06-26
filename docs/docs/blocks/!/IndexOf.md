# IndexOf

```clojure
(IndexOf
  :Item [(Any)]
  :All [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Item | `[(Any)]` |  | The item to find the index of from the input, if it's a sequence it will try to match all the items in the sequence, in sequence. |
| All | `[(Bool)]` |  | If true will return a sequence with all the indices of Item, empty sequence if not found. |


## Input
| Type | Description |
|------|-------------|
| `[(Seq [(Any)])]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Int)]` |  |


## Examples

```clojure
(IndexOf

)
```

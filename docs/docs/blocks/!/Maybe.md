# Maybe

```clojure
(Maybe
  :Blocks [(Block) (Seq [(Block)]) (None)]
  :Else [(Block) (Seq [(Block)]) (None)]
  :Silent [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Blocks | `[(Block) (Seq [(Block)]) (None)]` |  | The blocks to activate. |
| Else | `[(Block) (Seq [(Block)]) (None)]` |  | The blocks to activate on failure. |
| Silent | `[(Bool)]` |  | If logging should be disabled while running the blocks (this will also disable (Log) and (Msg) blocks) and no warning message should be printed on failure. |


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
(Maybe

)
```

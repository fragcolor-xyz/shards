# WhenNot

```clojure
(WhenNot
  :Predicate [(Block) (Seq [(Block)]) (None)]
  :Action [(Block) (Seq [(Block)]) (None)]
  :Passthrough [(Bool)]
)
```

## Definition
Conditonal block that only executes the action if the predicate is false.


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Predicate | `[(Block) (Seq [(Block)]) (None)]` |  | The predicate to evaluate in order to trigger `Action` (when `false`). |
| Action | `[(Block) (Seq [(Block)]) (None)]` |  | The blocks to activate on when `Predicate` is `false`. |
| Passthrough | `[(Bool)]` | `false` | The output of this block will be its input. |


## Input
| Type | Description |
|------|-------------|
| `[(Any)]` | The value that will be passed to the predicate. |


## Output
| Type | Description |
|------|-------------|
| `[(Any)]` | The input of the block if `Passthrough` is `true`, or the `Predicate` is `true`; otherwise, the output of the `Action`. |


## Examples

```clojure
(WhenNot
    ;:Predicate
    (IsMore 50)
    ;:Action
    (Msg "Less than 50")
)
```

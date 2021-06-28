# When

```clojure
(When
  :Predicate [(Block) (Seq [(Block)]) (None)]
  :Action [(Block) (Seq [(Block)]) (None)]
  :Passthrough [(Bool)]
)
```

## Definition
Conditonal block that only executes the action if the predicate is true.


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Predicate | `[(Block) (Seq [(Block)]) (None)]` | `None` | The predicate to evaluate in order to trigger Action. |
| Action | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks to activate on when Predicate is true for When and false for WhenNot. |
| Passthrough | `[(Bool)]` | `true` | The input of this block will be the output. |


## Input
| Type | Description |
|------|-------------|
| `[(Any)]` | The value that will be passed to the predicate. |


## Output
| Type | Description |
|------|-------------|
| `[(Any)]` | The input of the block if `Passthrough` is `true`, or the `Predicate` is `false`; otherwise, the output of the `Action`. |


## Examples

```clojure
(When
    ;:Predicate
    (IsMore 50)
    ;:Action
    (Msg "More than 50")
)
```

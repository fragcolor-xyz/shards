# When

```clojure
(When :Predicate (blocks) :Action (blocks) :Passthrough bool)
```

## Definition
Conditonal block that only executes the action if the predicate is true.

## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Action |  |  | The blocks to activate on when `Predicate` is `true`. |
| Passthrough | `bool` | `false` | The output of this block will be its input. |
| Predicate |  |  | The predicate to evaluate in order to trigger `Action` (when `true`). |

## Input
| Type | Description |
|------|-------------|
| Any | The value that will be passed to the predicate. |

## Output
| Type | Description |
|------|-------------|
| Any | The input of the block if `Passthrough` is `true`, or the `Predicate` is `false`; otherwise, the output of the `Action`. |

## Examples

```clojure
(When
    ;:Predicate
    (IsMore 50)
    ;:Action
    (Msg "More than 50")
)
```

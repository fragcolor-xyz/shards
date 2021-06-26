# WhenNot

```clojure
(WhenNot :Predicate (blocks) :Action (blocks) :Passthrough bool)
```

## Definition
Conditonal block that only executes the action if the predicate is false.

## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Action |  |  | The blocks to activate on when `Predicate` is `false`. |
| Passthrough | `bool` | `false` | The output of this block will be its input. |
| Predicate |  |  | The predicate to evaluate in order to trigger `Action` (when `false`). |

## Input
| Type | Description |
|------|-------------|
| Any | The value that will be passed to the predicate. |

## Output
| Type | Description |
|------|-------------|
| Any | The input of the block if `Passthrough` is `true`, or the `Predicate` is `true`; otherwise, the output of the `Action`. |

## Examples

```clojure
(WhenNot
    ;:Predicate
    (IsMore 50)
    ;:Action
    (Msg "Less than 50")
)
```

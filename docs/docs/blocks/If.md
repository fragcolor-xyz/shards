# If

```clojure
(If :Predicate (blocks) :Then (blocks) :Else (blocks) :Passthrough bool)
```

## Definition
Evaluates a predicate and executes an action.

## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Else |  |  | The blocks to activate when `Predicate` is `false`." |
| Passthrough | `bool` | `false` | The output of this block will be its input. |
| Predicate |  |  | The predicate to evaluate in order to trigger `Then` (when `true`) or `Else` (when `false`). |
| Then |  |  | The blocks to activate when `Predicate` is `true`.|

## Input
| Type | Description |
|------|-------------|
| Any | The value that will be passed to the predicate. |

## Output
| Type | Description |
|------|-------------|
| Any | The input of the block if `Passthrough` is `true`; otherwise, the output of the action that was performed (i.e. `Then` or `Else`). |

## Examples

```clojure
(If
    ;:Predicate
    (IsLess 2)
    ;:Then
    (Msg "lesser than 2")
    ;:Else
    (Msg "equal or greater than 2")
)
```

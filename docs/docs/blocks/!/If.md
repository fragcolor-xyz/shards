---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# If

```clojure
(If
  :Predicate [(Block) (Seq [(Block)]) (None)]
  :Then [(Block) (Seq [(Block)]) (None)]
  :Else [(Block) (Seq [(Block)]) (None)]
  :Passthrough [(Bool)]
)
```


## Definition

Evaluates a predicate and executes an action.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Predicate | `[(Block) (Seq [(Block)]) (None)]` | `None` | The predicate to evaluate in order to trigger `Then` (when `true`) or `Else` (when `false`). |
| Then | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks to activate when `Predicate` is `true`. |
| Else | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks to activate when `Predicate` is `false`. |
| Passthrough | `[(Bool)]` | `false` | The output of this block will be its input. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` | The value that will be passed to the predicate. |


## Output

| Type | Description |
|------|-------------|
| `[(Any)]` | The input of the block if `Passthrough` is `true`; otherwise, the output of the action that was performed (i.e. `Then` or `Else`). |


## Examples

```clojure
5
(If
 ;:Predicate
 (IsLess 2)
 ;:Then
 (Msg "lesser than 2")
 ;:Else
 (Msg "equal or greater than 2"))
```

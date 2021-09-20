---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


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
| Predicate | `[(Block) (Seq [(Block)]) (None)]` | `None` | The predicate to evaluate in order to trigger Action. |
| Action | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks to activate on when Predicate is true for When and false for WhenNot. |
| Passthrough | `[(Bool)]` | `true` | The output of this block will be its input. |


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
42
(WhenNot
 ;:Predicate
 (IsMore 50)
 ;:Action
 (Msg "Less than 50"))
```


--8<-- "includes/license.md"

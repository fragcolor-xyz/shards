# Cond

```clojure
(Cond
  :Chains [(Seq [(Block) (Seq [(Block)]) (None)])]
  :Passthrough [(Bool)]
  :Threading [(Bool)]
)
```

## Definition
Takes a sequence of conditions and predicates. Evaluates each condition one by one and if one matches, executes the associated action.


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chains | `[(Seq [(Block) (Seq [(Block)]) (None)])]` |  | A sequence of blocks, interleaving condition test predicate and action to execute if the condition matches. |
| Passthrough | `[(Bool)]` | `true` |  The output of this block will be its input. |
| Threading | `[(Bool)]` | `false` | Will not short circuit after the first true test expression. The threaded value gets used in only the action and not the test part of the clause. |


## Input
| Type | Description |
|------|-------------|
| `[(Any)]` | The value that will be passed to each predicate and action to execute. |


## Output
| Type | Description |
|------|-------------|
| `[(Any)]` | The input of the block if `Passthrough` is `true`; otherwise, the output of the action of the first matching condition. |


## Examples

```clojure
;; input is a bool
;; output is a string
(Cond
    ;:Chains
    [
        (-> true) "true"
        (-> false) "false"
    ]
    ;:Passthrough
    false
)
```

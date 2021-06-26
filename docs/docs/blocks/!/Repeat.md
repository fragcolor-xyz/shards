# Repeat

```clojure
(Repeat
  :Action [(Block) (Seq [(Block)])]
  :Times [(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])])]
  :Forever [(Bool)]
  :Until [(Block) (Seq [(Block)]) (None)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Action | `[(Block) (Seq [(Block)])]` |  | The blocks to repeat. |
| Times | `[(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])])]` |  | How many times we should repeat the action. |
| Forever | `[(Bool)]` |  | If we should repeat the action forever. |
| Until | `[(Block) (Seq [(Block)]) (None)]` |  | Optional blocks to use as predicate to continue repeating until it's true |


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
(Repeat

)
```

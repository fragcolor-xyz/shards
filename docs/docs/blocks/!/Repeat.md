---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


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
| Action | `[(Block) (Seq [(Block)])]` | `None` | The blocks to repeat. |
| Times | `[(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])])]` | `0` | How many times we should repeat the action. |
| Forever | `[(Bool)]` | `false` | If we should repeat the action forever. |
| Until | `[(Block) (Seq [(Block)]) (None)]` | `None` | Optional blocks to use as predicate to continue repeating until it's true |


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


--8<-- "includes/license.md"

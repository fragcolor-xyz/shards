---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Maybe

```clojure
(Maybe
  :Blocks [(Block) (Seq [(Block)]) (None)]
  :Else [(Block) (Seq [(Block)]) (None)]
  :Silent [(Bool)]
)
```


## Definition

Attempts to activate a block or a sequence of blocks. Upon failure, activate another block or sequence of blocks.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Blocks | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks to activate. |
| Else | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks to activate on failure. |
| Silent | `[(Bool)]` | `false` | If logging should be disabled while running the blocks (this will also disable (Log) and (Msg) blocks) and no warning message should be printed on failure. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` | Must match the input types of the first block in the sequence. |


## Output

| Type | Description |
|------|-------------|
| `[(Any)]` | Will match the output types of the first block of the sequence. |


## Examples

```clojure
[1 2]
(Maybe
 ;:Blocks
 (Take 3)
 ;:Else
 (Take 0)
 ;:Silent
 true)
```

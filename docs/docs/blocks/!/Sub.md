---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Sub

```clojure
(Sub
  :Blocks [(Block) (Seq [(Block)]) (None)]
)
```


## Definition

Activates a block or a sequence of blocks independently, without consuming the input. In other words, the ouput of the sub flow will be its input regardless of the blocks activated in this sub flow.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Blocks | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks to execute in the sub flow. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` | The value given to the block or sequence of blocks in this sub flow. |


## Output

| Type | Description |
|------|-------------|
| `[(Any)]` | The output of this block will be its input. |


## Examples

```clojure
3.14159
(Sub
 ;:Blocks
 (->
  (ToString)
  (Assert.Is "3.14159" true)))
```


--8<-- "includes/license.md"

---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Await

```clojure
(Await
  :Blocks [(Block) (Seq [(Block)]) (None)]
)
```


## Definition

Executes a block or a sequence of blocks asynchronously and awaits their completion.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Blocks | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks to activate. |


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
(Await
 ;:Blocks
 (Log "Hello worker!"))
```


--8<-- "includes/license.md"

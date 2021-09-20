---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Mutant

```clojure
(Mutant
  :Block [(Block)]
  :Indices [(Seq [(Int)])]
  :Mutations [(Seq [(Block) (Seq [(Block)]) (None)])]
  :Options [(None) (Table [(Any)])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Block | `[(Block)]` | `None` | The block to mutate. |
| Indices | `[(Seq [(Int)])]` | `None` | The parameter indices to mutate of the inner block. |
| Mutations | `[(Seq [(Block) (Seq [(Block)]) (None)])]` | `None` | Optional chains of blocks (or Nones) to call when mutating one of the parameters, if empty a default operator will be used. |
| Options | `[(None) (Table [(Any)])]` | `None` | Mutation options table - a table with mutation options. |


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
(Mutant

)
```


--8<-- "includes/license.md"

---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Match

```clojure
(Match
  :Cases [(Seq [(Any)])]
  :Passthrough [(Bool)]
)
```


## Definition

Compares the input with specific cases and activate the corresponding block. Cases are compared in declaring order.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Cases | `[(Seq [(Any)])]` | `[]` | The cases to match the input against, a nil/None case will match anything. |
| Passthrough | `[(Bool)]` | `true` | The output of this block will be its input. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` | The value to compare against. |


## Output

| Type | Description |
|------|-------------|
| `[(Any)]` | The output of the matched block, or the same value as inoput when `Passthrough` is `true`. |


## Examples

```clojure
false
(Match
 ;:Cases
 [true ~["Yes"]
  false ~["No"]]
 ;:Passthrough
 false)
```

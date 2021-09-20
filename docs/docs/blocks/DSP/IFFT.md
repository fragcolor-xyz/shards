---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# DSP.IFFT

--8<-- "includes/experimental.md"

```clojure
(DSP.IFFT
  :Audio [(Bool)]
  :Complex [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Audio | `[(Bool)]` | `false` | If the output should be an Audio chunk. |
| Complex | `[(Bool)]` | `false` | If the output should be complex numbers (only if not Audio). |


## Input

| Type | Description |
|------|-------------|
| `[(Seq [(Float2)])]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Seq [(Float)]) (Seq [(Float2)]) (Audio)]` |  |


## Examples

```clojure
(DSP.IFFT

)
```


--8<-- "includes/license.md"

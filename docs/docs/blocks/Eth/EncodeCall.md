---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Eth.EncodeCall

```clojure
(Eth.EncodeCall
  :ABI [(None) (String) (ContextVar [(String)])]
  :Name [(None) (String)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| ABI | `[(None) (String) (ContextVar [(String)])]` | `None` | The contract's json ABI. |
| Name | `[(None) (String)]` | `None` | The name of the method to call. |


## Input

| Type | Description |
|------|-------------|
| `[(Seq [(Any)])]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Bytes)]` |  |


## Examples

```clojure
(Eth.EncodeCall

)
```


--8<-- "includes/license.md"

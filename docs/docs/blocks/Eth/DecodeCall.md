---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Eth.DecodeCall

```clojure
(Eth.DecodeCall
  :ABI [(None) (String) (ContextVar [(String)])]
  :Name [(None) (String)]
  :Input [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| ABI | `[(None) (String) (ContextVar [(String)])]` | `None` | The contract's json ABI. |
| Name | `[(None) (String)]` | `None` | The name of the method to call. |
| Input | `[(Bool)]` | `false` | If the input is the actual function call transaction input rather than the result of the call. |


## Input

| Type | Description |
|------|-------------|
| `[(Bytes) (String)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Seq [(Any)])]` |  |


## Examples

```clojure
(Eth.DecodeCall

)
```

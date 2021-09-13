---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# ECDSA.Sign

```clojure
(ECDSA.Sign
  :Key [(Bytes) (ContextVar [(Bytes)]) (String) (ContextVar [(String)])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Key | `[(Bytes) (ContextVar [(Bytes)]) (String) (ContextVar [(String)])]` | `None` | The private key to be used to sign the hashed message input. |


## Input

| Type | Description |
|------|-------------|
| `[(Bytes)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Seq [(Bytes)])]` |  |


## Examples

```clojure
(ECDSA.Sign

)
```

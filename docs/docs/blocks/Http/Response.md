---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Http.Response

```clojure
(Http.Response
  :Status [(Int)]
  :Headers [(Table [(String)]) (ContextVar [(Table [(String)])]) (None)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Status | `[(Int)]` | `200` | The HTTP status code to return. |
| Headers | `[(Table [(String)]) (ContextVar [(Table [(String)])]) (None)]` | `None` | The headers to attach to this response. |


## Input

| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Examples

```clojure
(Http.Response

)
```

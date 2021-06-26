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
| Status | `[(Int)]` |  | The HTTP status code to return. |
| Headers | `[(Table [(String)]) (ContextVar [(Table [(String)])]) (None)]` |  | The headers to attach to this response. |


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

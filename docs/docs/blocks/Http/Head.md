# Http.Head

```clojure
(Http.Head
  :URL [(String) (ContextVar [(String)])]
  :Headers [(None) (Table [(String)]) (ContextVar [(Table [(String)])])]
  :Timeout [(Int)]
  :Bytes [(Bool)]
  :FullResponse [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| URL | `[(String) (ContextVar [(String)])]` |  | The url to request to. |
| Headers | `[(None) (Table [(String)]) (ContextVar [(Table [(String)])])]` |  | The headers to use for the request. |
| Timeout | `[(Int)]` |  | How many seconds to wait for the request to complete. |
| Bytes | `[(Bool)]` |  | If instead of a string the block should outout bytes. |
| FullResponse | `[(Bool)]` |  | If the output should be a table with the full response, including headers and status. |


## Input
| Type | Description |
|------|-------------|
| `[(None) (Table [(String)])]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Examples

```clojure
(Http.Head

)
```

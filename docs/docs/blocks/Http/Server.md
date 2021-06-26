# Http.Server

```clojure
(Http.Server
  :Handler [(Chain) (None)]
  :Endpoint [(String)]
  :Port [(Int)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Handler | `[(Chain) (None)]` |  | The chain that will be spawned and handle a remote request. |
| Endpoint | `[(String)]` |  | The URL from where your service can be accessed by a client. |
| Port | `[(Int)]` |  | The port this service will use. |


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
(Http.Server

)
```

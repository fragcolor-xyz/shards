---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


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
| Handler | `[(Chain) (None)]` | `None` | The chain that will be spawned and handle a remote request. |
| Endpoint | `[(String)]` | `"0.0.0.0"` | The URL from where your service can be accessed by a client. |
| Port | `[(Int)]` | `7070` | The port this service will use. |


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

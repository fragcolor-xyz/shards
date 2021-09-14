---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# WebSocket.Client

```clojure
(WebSocket.Client
  :Name [(String)]
  :Host [(String) (ContextVar [(String)])]
  :Target [(String) (ContextVar [(String)])]
  :Port [(Int) (ContextVar [(Int)])]
  :Secure [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Name | `[(String)]` | `""` | The name of this websocket instance. |
| Host | `[(String) (ContextVar [(String)])]` | `"echo.websocket.org"` | The remote host address or IP. |
| Target | `[(String) (ContextVar [(String)])]` | `"/"` | The remote host target path. |
| Port | `[(Int) (ContextVar [(Int)])]` | `443` | The remote host port. |
| Secure | `[(Bool)]` | `true` | If the connection should be secured. |


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
(WebSocket.Client

)
```

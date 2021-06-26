# Network.Server

```clojure
(Network.Server
  :Address [(String) (ContextVar [(String)])]
  :Port [(Int) (ContextVar [(Int)])]
  :Receive [(Block) (Seq [(Block)]) (None)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Address | `[(String) (ContextVar [(String)])]` |  | The local bind address or the remote address. |
| Port | `[(Int) (ContextVar [(Int)])]` |  | The port to bind if server or to connect to if client. |
| Receive | `[(Block) (Seq [(Block)]) (None)]` |  | The flow to execute when a packet is received. |


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
(Network.Server

)
```

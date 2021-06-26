# Stop

```clojure
(Stop
  :Chain [(Chain) (String) (None) (ContextVar [(Chain)])]
  :Passthrough [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chain | `[(Chain) (String) (None) (ContextVar [(Chain)])]` |  | The chain to wait. |
| Passthrough | `[(Bool)]` |  | The input of this block will be the output. |


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
(Stop

)
```

# Wait

```clojure
(Wait
  :Chain [(Chain) (String) (None) (ContextVar [(Chain)])]
  :Passthrough [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chain | `[(Chain) (String) (None) (ContextVar [(Chain)])]` | `None` | The chain to wait. |
| Passthrough | `[(Bool)]` | `false` | The input of this block will be the output. |


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
(Wait

)
```

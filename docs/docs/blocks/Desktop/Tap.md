# Desktop.Tap

```clojure
(Desktop.Tap
  :Window [(Object) (None)]
  :Long [(Bool)]
  :Natural [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Window | `[(Object) (None)]` | `None` | None or a window variable we wish to use as relative origin. |
| Long | `[(Bool)]` | `false` | A big delay will be injected after tap down to simulate a long tap. |
| Natural | `[(Bool)]` | `true` | Small pauses will be injected after tap events down & up. |


## Input
| Type | Description |
|------|-------------|
| `[(Int2)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Int2)]` |  |


## Examples

```clojure
(Desktop.Tap

)
```

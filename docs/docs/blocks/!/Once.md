# Once

```clojure
(Once
  :Action [(Block) (Seq [(Block)])]
  :Every [(Float)]
  :Serialized [(Bool)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Action | `[(Block) (Seq [(Block)])]` |  | The blocks to execute. |
| Every | `[(Float)]` |  | The number of seconds to wait until repeating the action, if 0 the action will happen only once per chain flow execution. |
| Serialized | `[(Bool)]` |  | If false, when the parent chain is serialized this block will be removed, the exposed variables in the current chain if populated will be serialized. |


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
(Once

)
```

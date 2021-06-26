# Process.Run

```clojure
(Process.Run
  :Executable [(Path) (String)]
  :Arguments [(None) (Seq [(String)]) (ContextVar [(Seq [(String)])])]
  :Timeout [(Int)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Executable | `[(Path) (String)]` |  | The executable to run. |
| Arguments | `[(None) (Seq [(String)]) (ContextVar [(Seq [(String)])])]` |  | The arguments to pass to the executable. |
| Timeout | `[(Int)]` |  | The maximum time to wait for the executable to finish in seconds. |


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
(Process.Run

)
```

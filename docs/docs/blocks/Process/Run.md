---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


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
| Executable | `[(Path) (String)]` | `""` | The executable to run. |
| Arguments | `[(None) (Seq [(String)]) (ContextVar [(Seq [(String)])])]` | `None` | The arguments to pass to the executable. |
| Timeout | `[(Int)]` | `30` | The maximum time to wait for the executable to finish in seconds. |


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

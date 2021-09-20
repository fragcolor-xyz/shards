---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Desktop.HasWindow

```clojure
(Desktop.HasWindow
  :Title [(String)]
  :Class [(String)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Title | `[(String)]` | `""` | The title of the window to look for. |
| Class | `[(String)]` | `""` | An optional and platform dependent window class. |


## Input

| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Bool)]` |  |


## Examples

```clojure
(Desktop.HasWindow

)
```


--8<-- "includes/license.md"

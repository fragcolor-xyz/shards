---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# FS.Write

```clojure
(FS.Write
  :Contents [(String) (Bytes) (ContextVar [(String)]) (ContextVar [(Bytes)]) (None)]
  :Overwrite [(Bool)]
  :Append [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Contents | `[(String) (Bytes) (ContextVar [(String)]) (ContextVar [(Bytes)]) (None)]` | `None` | The string or bytes to write as the file's contents. |
| Overwrite | `[(Bool)]` | `false` | Overwrite the file if it already exists. |
| Append | `[(Bool)]` | `false` | If we should append Contents to an existing file. |


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
(FS.Write

)
```

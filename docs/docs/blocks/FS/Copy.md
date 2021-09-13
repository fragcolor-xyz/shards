---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# FS.Copy

```clojure
(FS.Copy
  :Destination [(String) (ContextVar [(String)]) (None)]
  :Behavior [(Enum)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Destination | `[(String) (ContextVar [(String)]) (None)]` | `None` | The destination path, can be a file or a directory. |
| Behavior | `[(Enum)]` | `Enum: 0 vendor: 0x73696e6b type: 0x66736f77` | What to do when the destination already exists. |


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
(FS.Copy

)
```

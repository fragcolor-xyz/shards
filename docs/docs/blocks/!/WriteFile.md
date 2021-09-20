---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# WriteFile

```clojure
(WriteFile
  :File [(String) (ContextVar [(String)]) (None)]
  :Append [(Bool)]
  :Flush [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| File | `[(String) (ContextVar [(String)]) (None)]` | `None` | The file to read/write from. |
| Append | `[(Bool)]` | `false` | If we should append to the file if existed already or truncate. (default: false). |
| Flush | `[(Bool)]` | `false` | If the file should be flushed to disk after every write. |


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
(WriteFile

)
```


--8<-- "includes/license.md"

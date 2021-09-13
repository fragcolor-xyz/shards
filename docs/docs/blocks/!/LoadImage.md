---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# LoadImage

```clojure
(LoadImage
  :File [(String) (ContextVar [(String)]) (None)]
  :BPP [(Enum)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| File | `[(String) (ContextVar [(String)]) (None)]` | `None` | The file to read/write from. |
| BPP | `[(Enum)]` | `Enum: 0 vendor: 0x73696e6b type: 0x69627070` | bits per pixel (HDR images loading and such!) |


## Input

| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Image)]` |  |


## Examples

```clojure
(LoadImage

)
```

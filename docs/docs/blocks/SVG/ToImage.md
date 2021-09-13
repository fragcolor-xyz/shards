---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# SVG.ToImage

```clojure
(SVG.ToImage
  :Size [(Int2)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Size | `[(Int2)]` | `(0, 0)` | The desired output size, if (0, 0) will default to the size defined in the svg data. |


## Input

| Type | Description |
|------|-------------|
| `[(String) (Bytes)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Image)]` |  |


## Examples

```clojure
(SVG.ToImage

)
```

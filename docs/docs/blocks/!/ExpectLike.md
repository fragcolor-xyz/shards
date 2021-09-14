---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# ExpectLike

```clojure
(ExpectLike
  :Example [(Any)]
  :Unsafe [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Example | `[(Any)]` | `None` | The example value to expect, in the case of a used chain, the output type of that chain will be used. |
| Unsafe | `[(Bool)]` | `false` | If we should skip performing deep type hashing and comparison. (generally fast but this might improve performance) |


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
(ExpectLike

)
```

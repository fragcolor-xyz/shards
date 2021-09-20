---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# GFX.Screenshot

```clojure
(GFX.Screenshot
  :Overwrite [(Bool)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Overwrite | `[(Bool)]` | `true` | If each activation should overwrite the previous screenshot at the given path if existing. If false the path will be suffixed in order to avoid overwriting. |


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
(GFX.Screenshot

)
```


--8<-- "includes/license.md"

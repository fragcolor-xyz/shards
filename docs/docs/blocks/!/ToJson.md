---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# ToJson

```clojure
(ToJson
  :Pure [(Bool)]
  :Indent [(Int)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Pure | `[(Bool)]` | `true` | If the input string is generic pure json rather then chainblocks flavored json. |
| Indent | `[(Int)]` | `0` | How many spaces to use as json prettify indent. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(String)]` |  |


## Examples

```clojure
(ToJson

)
```


--8<-- "includes/license.md"

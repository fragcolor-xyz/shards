---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Do

```clojure
(Do
  :Chain [(Chain) (String) (None)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chain | `[(Chain) (String) (None)]` | `None` | The chain to run. |


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
(def logicChain
  (Chain
   "dologic"
   (IsMore 10) (Or) (IsLess 0)))

-10
(Do logicChain)
(Assert.Is true true)

5
(Do "dologic")
(Assert.IsNot true true)
```

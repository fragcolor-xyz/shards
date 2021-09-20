---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Step

```clojure
(Step
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
(defchain
  hello
  (Msg "Hello"))

(Detach hello)
(Step
;:Chain
 hello)
```


--8<-- "includes/license.md"

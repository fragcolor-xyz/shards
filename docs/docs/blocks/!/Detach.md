---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Detach

```clojure
(Detach
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

(Detach
 ;:Chain
 hello)
(Step hello)
```

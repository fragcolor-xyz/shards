---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Start

```clojure
(Start
  :Chain [(Chain) (String) (None)]
)
```


## Definition

Starts a given chain and suspends the current one. In other words, switches flow execution to another chain.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chain | `[(Chain) (String) (None)]` | `None` | The name of the chain to switch to. |


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
(defchain hello
   (Msg "Hello"))

(Start hello)
```


--8<-- "includes/license.md"

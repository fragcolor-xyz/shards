---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Branch

```clojure
(Branch
  :Chains [(Chain) (Seq [(Chain)]) (None)]
)
```


## Definition

A branch is a child node that runs and is ticked when this block is activated, chains on this node will inherit all of the available exposed variables in the activator chain.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chains | `[(Chain) (Seq [(Chain)]) (None)]` | `None` | The chains to schedule and run on this branch. |


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
(defchain c1
  .msg1 (Log))
(defchain c2
  .msg2 (Log))
(defchain branch
  "Hello" = .msg1
  "World" = .msg2
  (Branch [c1 c2]))

(Do branch)
```

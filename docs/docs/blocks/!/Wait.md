---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Wait

```clojure
(Wait
  :Chain [(Chain) (String) (None) (ContextVar [(Chain)])]
  :Passthrough [(Bool)]
)
```


## Definition

Waits for another chain to complete before resuming execution of the current chain.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chain | `[(Chain) (String) (None) (ContextVar [(Chain)])]` | `None` | The chain to wait. |
| Passthrough | `[(Bool)]` | `false` | The output of this block will be its input. |


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
  myChain
  (If (IsLess 0) (-> "Negative") (-> "Positive")))

5
(Detach myChain)
(Wait
 ;:Chain
 "myChain"
 ;:Passthrough
 false)
(Assert.Is "Positive" true)
```


--8<-- "includes/license.md"

---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Stop

```clojure
(Stop
  :Chain [(Chain) (String) (None) (ContextVar [(Chain)])]
  :Passthrough [(Bool)]
)
```


## Definition

Stops another chain. If no chain is given, stops the current chain.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chain | `[(Chain) (String) (None) (ContextVar [(Chain)])]` | `None` | The chain to stop. |
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
(defloop Loop
  (Math.Add 1)
  (Log)
  (Cond
   [(-> (Is 5))
    (Stop)])
  (Restart))

0
(Do Loop)
(Wait Loop)
```


--8<-- "includes/license.md"

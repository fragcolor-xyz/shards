---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Desktop.WaitKeyEvent

```clojure
(Desktop.WaitKeyEvent)
```


## Definition

### Pauses the chain and waits for keyboard events.
#### The output of this block will be a Int2.
 * The first integer will be 0 for Key down/push events and 1 for Key up/release events.
 * The second integer will the scancode of the key.



## Input

| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Int2)]` |  |


## Examples

```clojure
(Desktop.WaitKeyEvent

)
```

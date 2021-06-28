# Desktop.SendKeyEvent

```clojure
(Desktop.SendKeyEvent
  :Window [(Object) (None)]
)
```

## Definition
### Sends the input key event.
#### The input of this block will be a Int2.
 * The first integer will be 0 for Key down/push events and 1 for Key up/release events.
 * The second integer will the scancode of the key.


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Window | `[(Object) (None)]` | `None` | None or a window variable if we wish to send the event only to a specific target window. |


## Input
| Type | Description |
|------|-------------|
| `[(Int2)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Int2)]` |  |


## Examples

```clojure
(Desktop.SendKeyEvent

)
```

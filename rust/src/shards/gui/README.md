# egui integration

## Input & output

Note: Here we are talking about shard input, not keyboard or mouse input.

Depending on the type of UI elements, a shard might or might not consume its input, and it might or might not produce output.
Here we list rules of thumb to decide what an element should do with its input or output.

### UI context

The UI context object passes its input to the inner UI.

```clj
;; any input here
(UI
 .ui-draw-queue
 (-> ;; same input here
     (UI.Window)))
```

### Top-level containers*

They also pass their input to the inner UI contents. Usually that input is the same as the one received by the parent UI context.
They return their initial input as output.

E.g. `(UI.Window)`, `(UI.BottomPanel)`, `(UI.CentralPanel)`, `(UI.LeftPanel)`, `(UI.RightPanel)`, `(UI.TopPanel)`

```clj
;; any input here
(UI
 .ui-draw-queue
 (-> ;; same input here
     (UI.Window
      (-> ;; same input here
          (Log "input")))))
```

### Intermediate containers

Those containers are here to group or layout other widgets together. They only impact the visual organisation of other widgets. They could also be swapped from each other or removed altogether. In such case, they should pass their own input to the inner shards and return the output of those same inner shards.

E.g. `(UI.Group)`, `(UI.Scope)`, `(UI.Horizontal)`, `(UI.Vertical)`, `(UI.Grid)`, `(UI.Columns)`

Remarks: `(UI.BottomPanel)`, `(UI.CentralPanel)`, `(UI.LeftPanel)`, `(UI.RightPanel)` and `(UI.TopPanel)` can also be placed into other panels or within a window. For that reason, they can also be considered "intermediate containers" and do return the output from their inner shards in such case.

```clj
1 (Math.Add 2)
(UI.Horizontal
 (-> ;; input here is the result of 1 + 2
     (| "Result" (UI.Label))
     ;; label displays the input but doesn't consume it
     (UI.Label)
     ;; still the same input here
     (Log)
     ;; a different value
     42))
;; input is now 42
(Log)
```

### Actionable widgets

Widgets that respond to user interaction can sometimes return a `bool` indicating if they were clicked on or not. If they produce a value independent from their input, then we also flag their input type as `None` to indicate that aspect.

E.g. `(UI.Button)`, `(UI.Checkbox)`, `(UI.RadioButton)`

```clj
(UI.Window
 (-> (When (-> ;; input ignored here
               (UI.Button "Click me!")
               ;; button returns true when clicked
               (Log "click status")))))
;; output of Window is whether the button was clicked
```

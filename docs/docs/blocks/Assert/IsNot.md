---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Assert.IsNot

```clojure
(Assert.IsNot
  :Value [(Any)]
  :Abort [(Bool)]
)
```


## Definition

This assertion is used to check whether the input is different from a given value.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Value | `[(Any)]` | `None` | The value to test against for equality. |
| Abort | `[(Bool)]` | `false` | If we should abort the process on failure. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` | The input can be of any type. |


## Output

| Type | Description |
|------|-------------|
| `[(Any)]` | The output will be the input (passthrough). |


## Examples

```clojure
8
(Assert.IsNot
 ;:Value
 16
 ;:Abort
 true)
```


--8<-- "includes/license.md"

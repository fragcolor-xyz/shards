---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Math.Mean

```clojure
(Math.Mean
  :Kind [(Enum)]
)
```


## Definition

Calculates the mean of a sequence of floating point numbers.


## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Kind | `[(Enum)]` | `Enum: 0 vendor: 0x73696e6b type: 0x6d65616e` | The kind of Pythagorean means. |


## Input

| Type | Description |
|------|-------------|
| `[(Seq [(Float)])]` | A sequence of floating point numbers. |


## Output

| Type | Description |
|------|-------------|
| `[(Float)]` | The calculated mean. |


## Examples

```clojure
(Const [-1.0 0.0 1.0 2.0 5.0])
(Math.Mean
 ;:Kind
Mean.Arithmetic)
(Assert.Is 1.4 true)
```

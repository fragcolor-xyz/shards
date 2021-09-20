---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Recur

```clojure
(Recur)
```


## Definition




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
(defchain fibo
   >= .n
   (If (IsLess 2)
       :Then (-> (Pass))
       :Else (-> .n (Math.Subtract 2) (Recur) >= .a
                 .n (Math.Subtract 1) (Recur) >= .b
                 .a (Math.Add .b))))

16 (Do fibo)
(Log)
(Assert.Is 987 true)
```


--8<-- "includes/license.md"

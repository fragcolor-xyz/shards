---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Replace

```clojure
(Replace
  :Patterns [(None) (Seq [(String)]) (ContextVar [(Seq [(String)])]) (ContextVar [(Seq [(Any)])]) (Seq [(Any)])]
  :Replacements [(None) (Any) (ContextVar [(Any)]) (Seq [(Any)]) (ContextVar [(Seq [(Any)])])]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Patterns | `[(None) (Seq [(String)]) (ContextVar [(Seq [(String)])]) (ContextVar [(Seq [(Any)])]) (Seq [(Any)])]` | `None` | The patterns to find. |
| Replacements | `[(None) (Any) (ContextVar [(Any)]) (Seq [(Any)]) (ContextVar [(Seq [(Any)])])]` | `None` | The replacements to apply to the input, if a single value is provided every match will be replaced with that single value. |


## Input

| Type | Description |
|------|-------------|
| `[(Seq [(Any)]) (String)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Seq [(Any)]) (String)]` |  |


## Examples

```clojure
(Replace

)
```


--8<-- "includes/license.md"

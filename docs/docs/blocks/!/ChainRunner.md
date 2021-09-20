---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# ChainRunner

```clojure
(ChainRunner
  :Chain [(ContextVar [(Chain)])]
  :Mode [(Enum)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chain | `[(ContextVar [(Chain)])]` | `None` | The chain variable to compose and run. |
| Mode | `[(Enum)]` | `Enum: 0 vendor: 0x73696e6b type: 0x72756e43` | The way to run the chain. Inline: will run the sub chain inline within the root chain, a pause in the child chain will pause the root too; Detached: will run the chain separately in the same node, a pause in this chain will not pause the root; Stepped: the chain will run as a child, the root will tick the chain every activation of this block and so a child pause won't pause the root. |


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
(def! testChain
  (Chain
   "testChain"
   (Msg "Hello World from testChain!")))
(Const testChain)
(WriteFile "data/testChain.bin" :Flush true)

(ReadFile "data/testChain.bin")
(ExpectChain) >= .loadedChain
(ChainRunner
 ;:Chain 
 .loadedChain
 ;:Mode
 RunChainMode.Inline)
```


--8<-- "includes/license.md"

---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# ChainLoader

```clojure
(ChainLoader
  :Provider [(Object) (None)]
  :Mode [(Enum)]
  :OnReload [(Block) (Seq [(Block)]) (None)]
  :OnError [(Block) (Seq [(Block)]) (None)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Provider | `[(Object) (None)]` | `None` | The chainblocks chain provider. |
| Mode | `[(Enum)]` | `Enum: 0 vendor: 0x73696e6b type: 0x72756e43` | The way to run the chain. Inline: will run the sub chain inline within the root chain, a pause in the child chain will pause the root too; Detached: will run the chain separately in the same node, a pause in this chain will not pause the root; Stepped: the chain will run as a child, the root will tick the chain every activation of this block and so a child pause won't pause the root. |
| OnReload | `[(Block) (Seq [(Block)]) (None)]` | `None` | Blocks to execute when the chain is reloaded, the input of this flow will be the reloaded chain. |
| OnError | `[(Block) (Seq [(Block)]) (None)]` | `None` | Blocks to execute when a chain reload failed, the input of this flow will be the error message. |


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
;; loadme.edn
;; (Msg "Hello World from loadme.edn!")
;; (Stop)

(defloop loader
  (ChainLoader
   ;:Provider
   (Chain* "data/loadme.edn")
   ;:Mode
   RunChainMode.Inline))

(Start loader)
(Wait loader)
```


--8<-- "includes/license.md"

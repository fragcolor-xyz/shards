---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Audio.Channel

--8<-- "includes/experimental.md"

```clojure
(Audio.Channel
  :InputBus [(Int)]
  :InputChannels [(Seq [(Int)])]
  :OutputBus [(Int)]
  :OutputChannels [(Seq [(Int)])]
  :Volume [(Float) (ContextVar [(Float)])]
  :Blocks [(Block) (Seq [(Block)]) (None)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| InputBus | `[(Int)]` | `0` | The input bus number, 0 is the audio device ADC. |
| InputChannels | `[(Seq [(Int)])]` | `None` | The list of input channels to pass as input to Blocks. |
| OutputBus | `[(Int)]` | `0` | The output bus number, 0 is the audio device DAC. |
| OutputChannels | `[(Seq [(Int)])]` | `None` | The list of output channels to write from Blocks's output. |
| Volume | `[(Float) (ContextVar [(Float)])]` | `0.7` | The volume of this channel. |
| Blocks | `[(Block) (Seq [(Block)]) (None)]` | `None` | The blocks that will process audio data. |


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
(Audio.Channel

)
```


--8<-- "includes/license.md"

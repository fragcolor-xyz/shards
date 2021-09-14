---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Audio.Oscillator

```clojure
(Audio.Oscillator
  :Type [(Enum)]
  :Amplitude [(Float) (ContextVar [(Float)])]
  :Channels [(Int)]
  :SampleRate [(Int)]
  :Samples [(Int)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Type | `[(Enum)]` | `Enum: 0 vendor: 0x73696e6b type: 0x77617665` | The waveform type to oscillate. |
| Amplitude | `[(Float) (ContextVar [(Float)])]` | `0.4` | The waveform amplitude. |
| Channels | `[(Int)]` | `2` | The number of desired output audio channels. |
| SampleRate | `[(Int)]` | `44100` | The desired output sampling rate. Ignored if inside an Audio.Channel. |
| Samples | `[(Int)]` | `1024` | The desired number of samples in the output. Ignored if inside an Audio.Channel. |


## Input

| Type | Description |
|------|-------------|
| `[(Float)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Audio)]` |  |


## Examples

```clojure
(Audio.Oscillator

)
```

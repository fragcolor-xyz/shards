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
| Type | `[(Enum)]` |  | The waveform type to oscillate. |
| Amplitude | `[(Float) (ContextVar [(Float)])]` |  | The waveform amplitude. |
| Channels | `[(Int)]` |  | The number of desired output audio channels. |
| SampleRate | `[(Int)]` |  | The desired output sampling rate. Ignored if inside an Audio.Channel. |
| Samples | `[(Int)]` |  | The desired number of samples in the output. Ignored if inside an Audio.Channel. |


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

# Audio.ReadFile

```clojure
(Audio.ReadFile
  :File [(String) (ContextVar [(String)])]
  :Channels [(Int)]
  :SampleRate [(Int)]
  :Samples [(Int)]
  :Looped [(Bool)]
  :From [(Float) (ContextVar [(Float)]) (None)]
  :To [(Float) (ContextVar [(Float)]) (None)]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| File | `[(String) (ContextVar [(String)])]` |  | The audio file to read from (wav,ogg,mp3,flac). |
| Channels | `[(Int)]` |  | The number of desired output audio channels. |
| SampleRate | `[(Int)]` |  | The desired output sampling rate. Ignored if inside an Audio.Channel. |
| Samples | `[(Int)]` |  | The desired number of samples in the output. Ignored if inside an Audio.Channel. |
| Looped | `[(Bool)]` |  | If the file should be played in loop or should stop the chain when it ends. |
| From | `[(Float) (ContextVar [(Float)]) (None)]` |  | The starting time in seconds. |
| To | `[(Float) (ContextVar [(Float)]) (None)]` |  | The end time in seconds. |


## Input
| Type | Description |
|------|-------------|
| `[(None)]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Audio)]` |  |


## Examples

```clojure
(Audio.ReadFile

)
```

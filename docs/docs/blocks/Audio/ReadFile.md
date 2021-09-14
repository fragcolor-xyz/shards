---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


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
| File | `[(String) (ContextVar [(String)])]` | `None` | The audio file to read from (wav,ogg,mp3,flac). |
| Channels | `[(Int)]` | `2` | The number of desired output audio channels. |
| SampleRate | `[(Int)]` | `44100` | The desired output sampling rate. Ignored if inside an Audio.Channel. |
| Samples | `[(Int)]` | `1024` | The desired number of samples in the output. Ignored if inside an Audio.Channel. |
| Looped | `[(Bool)]` | `false` | If the file should be played in loop or should stop the chain when it ends. |
| From | `[(Float) (ContextVar [(Float)]) (None)]` | `None` | The starting time in seconds. |
| To | `[(Float) (ContextVar [(Float)]) (None)]` | `None` | The end time in seconds. |


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

---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Audio.WriteFile

```clojure
(Audio.WriteFile
  :File [(String) (ContextVar [(String)])]
  :Channels [(Int)]
  :SampleRate [(Int)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| File | `[(String) (ContextVar [(String)])]` | `None` | The audio file to read from (wav,ogg,mp3,flac). |
| Channels | `[(Int)]` | `2` | The number of desired output audio channels. |
| SampleRate | `[(Int)]` | `44100` | The desired output sampling rate. |


## Input

| Type | Description |
|------|-------------|
| `[(Audio)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Audio)]` |  |


## Examples

```clojure
(Audio.WriteFile

)
```

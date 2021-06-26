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
| File | `[(String) (ContextVar [(String)])]` |  | The audio file to read from (wav,ogg,mp3,flac). |
| Channels | `[(Int)]` |  | The number of desired output audio channels. |
| SampleRate | `[(Int)]` |  | The desired output sampling rate. |


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

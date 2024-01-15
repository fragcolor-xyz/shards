If you run `Once` with `Every:` set to its default value (i.e., 0), the sequence of shards will be executed only once per wire flow execution. Since this a very common use case (initialize loop counters, etc.) there's an alias for this - `Setup`.

Basically, `Setup` is just `Once` with its `Every:` parameter value permanently set to 0.

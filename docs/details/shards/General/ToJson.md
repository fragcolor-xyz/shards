If the `Pure ` parameter is set to false, the shard will accept an input of any type, but the output JSON will not be a standard JSON, but a Shards-specific JSON format that retains Shards type information.

If `Pure` is set to true, the shard will only accept basic JSON-compatible types: tables, sequences, strings, integers. floats, booleans, and null, but will return a standard JSON string.
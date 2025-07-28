This shard attempts to match the entire input string against the given pattern.
When using Regex.Match, ensure your pattern accounts for the entire string, using constructs like `.*` at the beginning or end if necessary.
If you need to capture specific parts of the string, use capture groups within a pattern that matches the full string.
For partial matches or finding substrings, use `Regex.Search` instead.
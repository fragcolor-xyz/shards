`Count` parses the value passed to it in the `:Name` parameter and returns the count of:
- characters (if a string was passed)
- elements (if a sequence was passed)
- key-value pairs (if a table was passed)

This shard does not have any defined input field and its output is the count value in `Int` type.

This shard will always return a zero count if applied to numbers.

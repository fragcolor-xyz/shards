The Item parameter can either accept a single value or a sequence of values.

If a single value is specified, the shard will return the index of the first occurrence of the value in the input sequence (If All is false) or a sequence of indices of all occurrences of the value in the input sequence (If All is true).

If a sequence of values is specified, it will compare the input sequence and check for occurrences of the specific pattern specified in the Item parameter. If All is set to true, it will return a sequence of indices of all occurrences of the pattern in the input sequence. If All is set to false, it will return the index of the first occurrence of the pattern in the input sequence.

Consider the following example:

```shards
[1 2 3 4 1 2 3 4 1 2 3 4]
IndexOf(Item: [1 2] All: true)
```

This will return `[0 4 8]` as the first occurrence of the pattern `[1 2]` is at index `0`, the next occurrence is at index `4` and the last occurrence is at index `8`. If all was set to false, it would return `0`.







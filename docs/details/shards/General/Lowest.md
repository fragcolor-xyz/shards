If the input sequence has elements that traditionally hold no value (eg. a sequence of strings), these elements will be given a lexicographical value for comparison.

If the input is a sequence with nested values or a table, the shard will also use lexicographical comparison to find the lowest value.

The input sequence can be of any type, but cannot be a sequence with elements of different types or be an empty sequence.
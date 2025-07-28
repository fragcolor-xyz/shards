Note that this shard works on any level of nesting. If the input sequence contains tables that also contains sequences for example, the shard will still place of the keys and values into a single sequence.

This shard also works on vectors (eg. @f2 @f3 @f4 @i2 @i3 @i4)

If a single table is provided as input, the shard will return a sequence with its keys and values as elements.

If a sequence with no nested values is provided, the shard will return the same sequence unchanged.

If a non-sequence is provided as input, the shard will return a sequence containing the input as the sole element.
This shard can take an integer or a sequence of integers as input. However, depending on the type of input, the appropriate `Operand` needs to be provided:

For non-sequence inputs: The `Operand` must match the input type exactly (e.g., Int2 with Int2, Color with Color).

For sequence inputs: The `Operand` can be either:
  - A matching non-sequence type (e.g., sequence of Int2 with a single Int2 `Operand`). Each element of the input sequence is operated on by the `Operand`.
  - Another sequence of elements with the same types. Each element of the `Operand` sequence is applied to the corresponding element of the input sequence. If the input sequence is longer, the `Operand` sequence will loop over till all elements of the input sequence are operated on. If the `Operand` sequence is longer, the extra elements of the `Operand` sequence are ignored.
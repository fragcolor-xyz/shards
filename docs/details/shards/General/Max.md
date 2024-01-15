This shard compares the input to its `Operand:` parameter and outputs the greater of the two values.

If the input is a sequence and the `Operand:` a single number, each input element is compared with the `Operand:` and the greater value from each comparison is collected and output as a sequence.

If the input and `Operand:` both are sequences, each element of the input sequence is compared with the corresponding-index element in the `Operand:` sequence and the greater value from each comparison is collected and output as a sequence.

If the input sequence is smaller in size than the `Operand:` sequence the comparison stops with the last element of the input sequence. If the `Operand:` sequence is smaller the remaining input sequence elements loop over the `Operand:` sequence till all input sequence elements have been compared.

This shard works only on numbers (integers, floats) and on sequences of such numbers. The data types of the input and the `Operand:` parameters must match.

!!! note "See also"
    - [`Min`](../Min)

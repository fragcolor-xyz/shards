This shard also is type sensitive and will only compare between input and operands of the same type. (e.g., 1 | Is(1.0) will throw a validation error).

Note that for two sequences to be considered equal or identical they must both contain the same elements and in the same order.

For two tables to be considered equal/identical they must both contain the same key/value pairs but the order of these pairs is irrelevant.

!!! note "See also"
    - [`IsNot`](../IsNot)

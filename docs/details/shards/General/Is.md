This shard compares the input to its `:Value` parameter and outputs `true` if they are equal or same, else outputs `false`.

If the input and `:Value` parameter have different data types they will be assessed as inequal by default even if they are numerically equal (for example `int 5` is not equal to `float 5.0`).

Note that for two sequences to be considered equal or identical they must both contain the same elements and in the same order.

For two tables to be considered equal/identical they must both contain the same key/value pairs but the order of these pairs is irrelevant.

!!! note "See also"
    - [`IsNot`](../IsNot)

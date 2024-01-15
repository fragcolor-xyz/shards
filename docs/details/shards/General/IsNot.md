This shard compares the input to its `Value:` parameter and outputs `true` if they are different, else outputs `false`.

If the input and `Value:` parameter have different data types they will be assessed as inequal by default even if they are numerically equal (for example `int 5` is not equal to `float 5.0`).

!!! note "See also"
    - [`Is`](../Is)

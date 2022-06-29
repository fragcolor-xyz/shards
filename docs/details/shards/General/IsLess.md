This shard compares the input to its `:Value` parameter and outputs `true` if the input value is less than the value of its `:Value` parameter, else it outputs `false`.

For a valid comparison, the input and `:Value` parameter must have identical data types. A comparison across different data types will throw a validation error.

This shard can be used to compare simple data types like stings, integers, floats, etc. However, complex data types like sequences or tables cannot be meaningfully compared for the lesser-than/greater-than attribute. Using this shard on such complex data types will give unexpected and meaningless results.

!!! note "See also"
    - [`IsLessEqual`](../IsLessEqual)
    - [`IsMore`](../IsMore)
    - [`IsMoreEqual`](../IsMoreEqual)

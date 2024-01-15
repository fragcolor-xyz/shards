`Const` declares a constant value (of any data type) by passing it into the parameter `Value:`. Such a value is usually declared for use as input in other shards.

A constant value declared with `Const` is un-named i.e., it is not assigned to any variable or allocated any alias. Hence it cannot be invoked or referred to later. To create named constants see [`Ref`](../Ref).

You can even skip this shard and pass the constant value directly but internally it will be translated to a `Const`shard that outputs this constant value. However, it's good practice to use this keyword in while passing constants in Shards programs.

You can also use `Const(nil)` to overwrite/nullify (since `nil` stands for null data) the input to the subsequent shard, if required (see the last code sample).

Input field is ignored and the output of this shard is the constant value defined by it.

!!! note "See also"
    - [`Ref`](../Ref)
    - [`Set`](../Set)

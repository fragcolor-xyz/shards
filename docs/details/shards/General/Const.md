`Const` declares a constant value (of any data type), usually for use within a shard as appropriate an input or a parameter value.

A constant value declared with `Const` is un-named i.e., it is not assigned to any variable or allocated any alias. Hence it cannot be invoked or referred to later.

If you want to create a named constant, see [`Ref`](../Ref).

You can even skip this shard and pass the constant value directly but internally it will be translated to a `Const`shard that outputs this constant value. 

However, it's good practice to use this keyword in while passing constants in Shards programs.

!!! note "See also"
    - [`Ref`](../Ref)
    - [`Set`](../Set)

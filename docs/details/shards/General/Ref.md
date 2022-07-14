`Ref` creates an immutable variable and assigns a constant value to it. Once created this variable cannot be changed.

The name of the variable comes from the `:Name` parameter and the constant value comes from the input. The type of input controls the kind of variable that will created: numeric input creates numeric variable, string input creates string variable, and sequence input would create a sequence variable.

To create a table variable, along with the input, you also have to pass the key in the `:Key` parameter. In this case the input (whatever it may be - numeric, string, sequence) becomes the value of the key that was passed in parameter `:Key`.

The `:Global` parameter controls whether the created variables can be referenced across wires (`:Global` set to `true`) or only within the current wire (`:Global` set to `false`, default behaviour).

The input to this shard is used as the value for the new variable and is also passed through as this shard's output.

!!! note
    `Ref` has two aliases: `=` and `&>`. Both are aliases for `(Ref ... :Global false)`. See the code examples at the end to understand how these aliases are used.

!!! note "See also"
    - [`Set`](../Set)
    - [`Get`](../Get)
    - [`Const`](../Const)

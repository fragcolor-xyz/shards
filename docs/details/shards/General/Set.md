`Set` creates a mutable variable and assigns a value to it. Once created this variable can be modified.

The name of the variable comes from the `:Name` parameter and the variable value comes from the input. The type of input controls the kind of variable that will created: numeric input creates numeric variable, string input creates string variable, and sequence input would create a sequence variable.

To create a table variable, along with the input, you also have to pass the key in the `:Key` parameter. In this case the input (whatever it may be - numeric, string, sequence) becomes the value of the key that was passed in parameter `:Key`.

The `:Global` parameter controls whether the created variables can be referenced across wires (`:Global` set to `true`) or only within the current wire (`:Global` set to `false`, default behaviour).

Though it will generate a warning `Set` can also be used to update existing variables (like adding a new key-value pair to an existing table).  

The input to this shard is used as the value for the variable being created and is also passed through as this shard's output.

!!! note
    `Set` has two aliases: `>=` is an alias for `(Set ... :Global false)` while `>==` is an alias for `(Set ... :Global true)`. See the code examples at the end to understand how these aliases are used.

!!! note "See also"
    - [`Ref`](../Ref)
    - [`Get`](../Get)
    - [`Const`](../Const)
    - [`Sequence`](../Sequence)
    - [`Table`](../Table)

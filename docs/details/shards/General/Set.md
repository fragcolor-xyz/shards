If the`Key` parameter is specified, `Set` will create a table variable. The input  becomes the value of the key that was passed in parameter `Key`.

The `Global:` parameter controls whether the created variables can be referenced across wires (`:Global` set to `true`) or only within the current wire (`Global:` set to `false`, default behaviour).

Though it will generate a warning `Set` can also be used to update existing variables (like adding a new key-value pair to an existing table, or updating the value of a key in an existing table).

Variables may be locally scoped (created with `(Global: false)`; exists only for current wire) or globally scoped (created with `(Global: true)`; exists for all wires of that mesh). Hence, in update mode (i.e. when you apply `Set` to an existing variable) the `Global` parameter is used in conjunction with the `Name` parameter to identify the correct variable to update. 

!!! note
    `Set` has an alias `>=`.

!!! note "See also"
    - [`AppendTo`](../AppendTo)
    - [`Const`](../Const)
    - [`Get`](../Get)
    - [`PrependTo`](../PrependTo)
    - [`Ref`](../Ref)
    - [`Sequence`](../Sequence)
    - [`Table`](../Table)

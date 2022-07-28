`Update` modifies the value of an existing mutable variable.

The name of the variable comes from the `:Name` parameter and the update value comes from the input. 

`Update` overwrites string, numeric, and sequence variables with the new value (coming from input). However, for sequences, it cannot update a sequence at the level of elements (i.e., add elements, remove elements, change element order, etc.), so it overwrites the whole sequence with whatever you've passed in the input field.

Also, for an existing table, `Update` can only change the existing keys' values. It cannot add new key-value pairs to the table (do that with [`Set`](../Set)). To update existing key-values in a table you need to pass the key in the `:Key` parameter.

Since variables may be locally scoped (created with `(:Global false)`; exists only for current wire) or globally scoped (created with `(:Global true)`; exists for all wires of that mesh), both parameters `:Global` and `:Name` are used in combination to identify the correct variable to update.

The input to this shard is the update value to be applied to the mutable variables and is also passed through as this shard's output.

!!! note
    `Update` has an alias `>`. Its an alias for `Update` with the defaults: `(Update ...)`. See the code examples at the end to understand how this alias is used.

!!! note "See also"
    - [`Set`](../Set)
    - [`Push`](../Push)
    - [`AppendTo`](../AppendTo)
    - [`PrependTo`](../PrependTo)
    - [`Sequence`](../Sequence)
    - [`Table`](../Table)

`Get`is used read the value of an existing variable. All sorts of variables that can be created by `Set`, can be read by `Get`. 

The `:Name` parameter should contain the name of the variable that's being read. If the variable is a string/numeric variable or a sequence it will be read directly. But for a table to be read its key should be passed in the parameter `:Key`. This will allow `Get` to access the particular key in the table and read it's value.

The `:Default` parameter specifies a value to return in case the variable being read doesn't yeild a valid value, or the sequence is malformed, or the required key is missing from the table, etc. This allows the program to continue processing even if some expected data is missing.

Since variables may be locally scoped (created with `(:Global false)`; exists only for current wire) or globally scoped (created with `(:Global true)`; exists for all wires of that mesh), both parameters `:Global` and `:Name` are used in combination to identify the correct variable to read.

Any input to this shard is ignored and its output contains the value of the variable read by `Get`.

!!! note
    `Get` has an alias `??`. This symbol represents the logical operator `OR`. Hence, `??` functions as an alias for `Get` with a default value. For example, `.var1 ?? 40` means `.var1 or 40` and this is effectively an alias for `(Get .var1 :Default 40)`. See the code examples at the end to understand how this alias is used.

!!! note "See also"
    - [`Ref`](../Ref)
    - [`Set`](../Get)
    - [`Const`](../Const)
    - [`Sequence`](../Sequence)
    - [`Table`](../Table)

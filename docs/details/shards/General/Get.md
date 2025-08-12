The parameter `Key`allows `Get` to access a specific key in the table and read it's value.

The `Default` parameter specifies a value to return in case the variable being read doesn't yeild a valid value, or the sequence is malformed, or the required key is missing from the table, etc. This allows the program to continue processing even if some expected data is missing.

Since variables may be locally scoped (created with `Global: false`; exists only for current wire) or globally scoped (created with `Global: true`; exists for all wires of that mesh), both parameters `Global` and `Name` are used in combination to identify the correct variable to drop elements from.

!!! note "See also"
    - [`Const`](../Const)
    - [`Ref`](../Ref)
    - [`Sequence`](../Sequence)
    - [`Set`](../Get)
    - [`Table`](../Table)

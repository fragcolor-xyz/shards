If the variable to clear is a table, the `Key` parameter specifies the key to clear. The value of this key still needs to be a sequence or a table. Otherwise, if the `Key` parameter `Clear` empties the whole table.

Since variables may be locally scoped (created with `Global: false`; exists only for current wire) or globally scoped (created with `Global: true`; exists for all wires of that mesh), both parameters `Global` and `Name` are used in combination to identify the correct variable to clear. 

Any input to this shard is ignored and instead passed through as its output.

!!! note "See also"
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Erase`](../Erase)
    - [`Pop`](../Pop)
    - [`PopFront`](../PopFront)
    - [`Remove`](../Remove)

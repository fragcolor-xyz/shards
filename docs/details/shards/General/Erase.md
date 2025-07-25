The `Indices` parameter specifies which elements or keys to remove from the target sequence or table. For example, `Erase(Indices: [0 1] Name: sequence)` removes the elements at indices 0 and 1 from a sequence, while `Erase(Indices: ["key1" "key2"] Name: table) deletes the keys "key1" and "key2" (and their values)` from a table.

The `Key:` parameter identifies which key contains the value to be modified. The value associated with this key must itself be a table or a sequence.

Since variables may be locally scoped (created with `Global: false`; exists only for current wire) or globally scoped (created with `Global: true`; exists for all wires of that mesh), both parameters `Global` and `Name` are used in combination to identify the correct variable to drop elements from.

!!! note "See also"
    - [`Clear`](../Clear)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Pop`](../Pop)
    - [`PopFront`](../PopFront)
    - [`Remove`](../Remove)

If the variable is a table, the `Key` parameter identifies which key to target. The value associated with this key must still be a sequence.

Since variables may be locally scoped (created with `Global: false`; exists only for current wire) or globally scoped (created with `Global: true`; exists for all wires of that mesh), both parameters `Global` and `Name` are used in combination to identify the correct variable to drop elements from.

!!! note "See also"
    - [`Clear`](../Clear)
    - [`Drop`](../Drop)
    - [`Erase`](../Erase)
    - [`Pop`](../Pop)
    - [`PopFront`](../PopFront)
    - [`PrependTo`](../PrependTo)
    - [`Remove`](../Remove)

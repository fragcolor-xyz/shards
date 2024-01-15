`DropFront` drops (removes) the first element of the sequence variable that has been passed to in the `Name:` parameter. 

This shard works on both sequences and tables. Parameter `Key:` applies only to tables.

Since variables may be locally scoped (created with `(Global: false)`; exists only for current wire) or globally scoped (created with `(Global: true)`; exists for all wires of that mesh), both parameters `Global:` and `Name:` are used in combination to identify the correct variable to drop elements from.

Any input to this shard is ignored and instead passed through as its output.

!!! note "See also"
    - [`Clear`](../Clear)
    - [`Drop`](../Drop)
    - [`Erase`](../Erase)
    - [`Pop`](../Pop)
    - [`PopFront`](../PopFront)
    - [`PrependTo`](../PrependTo)
    - [`Remove`](../Remove)

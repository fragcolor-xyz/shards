`Erase` deletes single or multiple elements (from sequences) and key-value pairs (from tables). 

For a sequence, this shard expects the index (or a sequence of indices in descending order) of the element(s) to be erased, followed by the name of the sequence variable in the `Name:` parameter.

For a table, this shard expects the key (or a sequence of keys) of the key-value pair(s) to be erased, followed by the name of the table variable in the `Name:` parameter. 

This shard works on both sequences and tables. Parameter `Key:` applies only to tables.

Since variables may be locally scoped (created with `(Global: false)`; exists only for current wire) or globally scoped (created with `(Global: true)`; exists for all wires of that mesh), both parameters `Global:` and `Name:` are used in combination to identify the correct variable to erase. 

Any input to this shard is ignored and instead passed through as its output.

!!! note "See also"
    - [`Clear`](../Clear)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Pop`](../Pop)
    - [`PopFront`](../PopFront)
    - [`Remove`](../Remove)

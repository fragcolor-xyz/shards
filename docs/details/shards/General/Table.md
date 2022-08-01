`Table` creates an empty table with or without a specified key (via the `:Key` parameter). The created table name is defined in the `:Name` parameter.

Whether the created table variable has a global scope (available to all wires on the mesh) or a local scope (available only to the wire its defined in) can be controlled via the `:Global` parameter (`true` for global scope, `false` for local scope; default is `false`).

In addition to the key and the scope, this shard can also define the table's inner data types via the `:Types` parameter. More than one data type may be set.

Any input to this shard is ignored and instead passed through as its output.

!!! note "See also"
    - [`Get`](../Get)
    - [`Push`](../Push)
    - [`Sequence`](../Sequence)
    - [`Set`](../Set)
    - [`Update`](../Update)

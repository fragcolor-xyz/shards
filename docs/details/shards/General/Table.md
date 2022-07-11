`Table` creates an empty table with or without a specified key (via the `:Key` parameter). 

Whether the created table variable has a global scope (available to all wires on the mesh) or a local scope (available only to the wire its defined in) can be controlled via the `:Global` parameter (`true` for global scope, `false` for local scope; default is `false`).

In addition to the key and the scope, this shard can also define the table's inner data types via the `:Types` parameter. More than one data type may be set.

Input/output fields are not applicable for this shard.

!!! note "See also"
    - [`Sequence`](../Sequence)

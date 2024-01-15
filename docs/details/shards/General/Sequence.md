`Sequence` creates an empty sequence when the `Key:` parameter is not set. If a key is passed via this parameter `Sequence` creates an empty table instead (behaving like the [`Table`](../Table) shard). The created sequence name is defined in the `Name:` parameter.

This shard can control the scope of the created sequence variable. A `true` value for the `Global:` parameter makes the scope of the sequence global (available to all wires on the mesh), and a `false` value makes the scope local (available only to the wire its defined in).

By default a sequence created with this shard would be cleared (emptied) every time the wire is executed (since `Clear:` is `true` by default). To retain the sequence values across wire iterations set the `Clear:` parameter to `false`.

This shard can also define the sequence's inner data types via the `Types:` parameter. More than one data type may be set.

Any input to this shard is ignored and instead passed through as its output.

!!! note "See also"
    - [`Get`](../Get)
    - [`Push`](../Push)
    - [`Set`](../Set)
    - [`Table`](../Table)
    - [`Update`](../Update)

For existing sequences `Push` pushes in the new element. If a sequence doesn't exist then `Push` will create it while pushing in the first element.

If the variable to be updated is a table, a key needs to be specified in the `Key` parameter. If the key does not exist, `Push` will create a new key and a new sequence for its value.

The `Global` parameter controls whether the created by `Push` can be referenced across wires (`Global` set to `true`) or only within the current wire (`Global` set to `false`, default behaviour).

Variables may be locally scoped (created with `(Global: false)`; exists only for current wire) or globally scoped (created with `(Global: true)`; exists for all wires of that mesh). Hence, is `Push` is updating an existing variable, the `Global` parameter is used in conjunction with the `Name` parameter to identify the correct variable to update. 


!!! note
    `Push` has an two alias: `>>`

!!! note "See also"
    - [`AppendTo`](../AppendTo)
    - [`PrependTo`](../PrependTo)
    - [`Sequence`](../Sequence)
    - [`Set`](../Set)
    - [`Table`](../Table)
    - [`Update`](../Update)

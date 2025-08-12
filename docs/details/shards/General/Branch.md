All child wires scheduled inherits and uses the context variables of the parent wire. Any changes to said variables will also be reflected on the parent wire and subsequent child wires scheduled.

Child wires are scheduled and executes inline. The parent wire will progress the state of all child wires sequentially, however, if there is any pauses or breaks in the child wires' execution, the shard will progress the state of the next child wire or relinquish control back to the parent wire. This means that any pauses on any child wire will not pause the parent wire.

Child wires are scheduled on a child mesh of the mesh the parent wire is on.
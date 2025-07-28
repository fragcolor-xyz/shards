The child wire scheduled inherits and uses the context variables of the parent wire. Any changes to said variables will also be reflected on the parent wire.

Child wire is scheduled and executes inline. However, the shard will only progress the state of the wire with each call and relinquish control back to the parent wire if there is any pause or break in the wire's execution. This means that any pauses on the child wire will not pause the parent wire.

Child wire is scheduled on the same mesh as the parent wire.
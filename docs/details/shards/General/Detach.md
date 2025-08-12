The child wire scheduled makes copies of the context variables of the parent wire. Any changes to said variables will not be reflected on the parent wire.

Child wire is scheduled asynchronously and executes independently of the parent wire. Any pauses on the child wire will not pause the parent wire.

Child wire is scheduled on the same mesh as the parent wire.

And only schedule one unique copy of a wire at a time. Meaning, if there is another call for `Detach` to detach the exact same wire, it will be ignored if the said wire is still running.
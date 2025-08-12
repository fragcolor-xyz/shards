The child wire scheduled makes copies of the context variables of the parent wire. Any changes to said variables will not be reflected on the parent wire.

Child wire is scheduled asynchronously and executes independently of the parent wire. Any pauses on the child wire will not pause the parent wire.

Child wire is scheduled on the same mesh as the parent wire.

Multiple copies of the specified wire can be scheduled. Everytime spawn is called, it will create and schedule a new copy of the wire.
All child wires scheduled inherits and uses the context variables of the parent wire. Any changes to said variables will also be reflected on the parent wire and subsequent child wires scheduled.

Child wires are scheduled and executes inline. The parent wire will execute all child wires sequentially, it will only continue its execution once all child wires have finished.

Child wires are scheduled on the same mesh as the parent wire.
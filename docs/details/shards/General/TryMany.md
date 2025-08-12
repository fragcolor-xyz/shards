All child wires scheduled makes copies of the parent wire's context variables. Any changes to said variables will not be reflected on the parent wire or other sibling wires scheduled using this shard.

Child wires are scheduled and executes inline. However, all child wires scheduled using this shard will execute in paralell. This shard however, will wait for all child wires to finish executing before it will continue its execution. Therefore any pauses on any child wire will also pause the parent wire.

Child wires are scheduled on the same mesh as the parent wire.

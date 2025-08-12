`Match` compares its input with every case declared via the `Cases` parameter (in the order of their declaration) till a match is found.

Once a match is found the shard of that matched case is activated/executed and `Match` execution stops. All subsequent cases (even matching ones) are ignored.

A `none` case matches anything, so it's a good practice to declare a `none` case at the end of `Cases` to execute some default logic if no valid matches exist for a given input. If you do not have a `none` case, then a non-matching input to `Match` will fail the shard.

### A note on `Passthrough` ###

The `Passthrough` parameter can control the final output of the shard it applies to.

Setting this parameter to `true` allows the original input of a shard to pass through as its output as well. If this parameter is set to `false`, passthrough is suppressed and then the output of the shard is the actual computed value coming out from the shard execution. When set to false, the output of all cases needs to be of the same type.

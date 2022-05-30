`Match` compares its input with every case declared via the `:Cases` parameter (in the order of their declaration) till a match is found.

Once a match is found the shard of that matched case is activated/executed and `Match` execution stops. All subsequent cases (even matching ones) are ignored.

A `nil` case matches with everything. Hence by declaring a `nil` case at the end of `:Cases` you can execute code for the condition when no matching cases were found.

### A note on `:Passthrough` ###

The `:Passthrough` parameter can control the final output of the shard it applies to.

Setting this parameter to `true` allows the original input of shard to passthrough as its output as well. If this paramter is set to `false`, passthrough is suppressed and then the output of the shard is the actual computed value coming out from the shard execution.

For `Match`, this parameter when set to `true` will allow the original input (the one that was used to match against every case in the shard) to appear again its final output (irrespective of the case match results). Setting `:Passthrough` to `false` will enable the matched case's shard output to appear as the output of `Match`.

However, for `:Passthrough` to work correctly, the data types of the shard input and the shard's case outputs must match.

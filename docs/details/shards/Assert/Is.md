If the input is equal to or the same as the `Value` parameter, the assertion is satisfied and the program will be allowed to continue. Control is then passed on to the next shard, irrespective of the `Abort` parameter.

When the assertion fails:

- If `Abort` is `true`, the program aborts with an error dump.

- If `Abort` is `false`, the program logs an assertion validation error but continues running. Control is passed to the next Wire scheduled on the Mesh.

Since this shard can control the conditions under which a program is allowed to run or be aborted, it is useful for writing unit test cases.

!!! note "See also"
    - [`Assert.IsAlmost`](../IsAlmost)
    - [`Assert.IsNot`](../IsNot)

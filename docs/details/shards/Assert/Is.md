If the assertion is satisfied (i.e., the input is equal to or same as the `:Value` parameter) the program will is allowed to continue (control passes to the next shard), irrespective of the `:Abort` parameter .

However, if the assertion fails, the program 
- aborts with an error dump if the `:Abort` parameter is set to `true`
- logs an assertion validation error but continues running (control passes to the next wire scheduled on the mesh).

Since this shard can precisely control the conditions under which a program is allowed to run or is to be aborted, it's effective for writing (inline) unit test cases with it.

!!! note "See also"
    - [`Assert.IsNot`](../IsNot)

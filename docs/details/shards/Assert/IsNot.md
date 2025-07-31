Returns true if the input is not equal to the `Value` parameter and false otherwise. The shard also is type sensitive (e.g., 1 | IsNot(1.0) will return true).

If the `Break` parameter is set to false - logs an assertion validation error but continues running the programme.

!!! note "See also"
    - [`Assert.Is`](../Is)
    - [`Assert.IsAlmost`](../IsAlmost)

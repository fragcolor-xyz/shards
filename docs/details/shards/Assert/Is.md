Returns true if the input is equal to the `Value:` parameter and false otherwise. The shard also is type sensitive (e.g., 1 | Is(1.0) will return false).

If the `Break` parameter is set to true - logs an assertion validation error but continues running the programme.

!!! note "See also"
    - [`Assert.IsAlmost`](../IsAlmost)
    - [`Assert.IsNot`](../IsNot)

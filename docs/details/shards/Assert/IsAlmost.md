Checks whether the input value lies within the range defined by the `Threshold:` parameter around the specified `Value:`. The shard returns true if the input is within this range (exclusive of the lower limit and inclusive of the upper limit), and false otherwise.

If the `Break` parameter is set to true - logs an assertion validation error but continues running the programme.
 
!!! note "See also"
    - [`Assert.Is`](../Is)
    - [`Assert.IsNot`](../IsNot)

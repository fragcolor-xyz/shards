This shard works on both sequences and tables. Parameter `Key:` applies only to tables.

Since variables may be locally scoped (created with `(Global: false)`; exists only for current wire) or globally scoped (created with `(Global: true)`; exists for all wires of that mesh), both parameters `Global` and `Name` are used in combination to identify the correct variable to pop elements from. 

!!! note "See also"
    - [`Clear`](../Clear)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Erase`](../Erase)
    - [`Pop`](../Pop)
    - [`PrependTo`](../PrependTo)
    - [`Remove`](../Remove)
   

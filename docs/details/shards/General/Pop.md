`Pop` drops (removes) the last element of the sequence variable that has been passed to in the `:Name` parameter and makes it available to the next shard as its input.

This shard works on both sequences and tables. Parameter `:Key` applies only to tables.

Since variables may be locally scoped (created with `(:Global false)`; exists only for current wire) or globally scoped (created with `(:Global true)`; exists for all wires of that mesh), both parameters `:Global` and `:Name` are used in combination to identify the correct variable to pop elements from. 

Input field is ignored and the output of this shard is the element which was popped from the sequence passed to it via the `:Name` parameter. 

!!! note "See also"
    - [`AppendTo`](../AppendTo)
    - [`Clear`](../Clear)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Erase`](../Erase)
    - [`PopFront`](../PopFront)
    - [`Remove`](../Remove)
   

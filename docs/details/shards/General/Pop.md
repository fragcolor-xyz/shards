If the variable is a table, the `Key` parameter identifies which key to target. The value associated with this key must still be a sequence.

Since variables may be locally scoped (created with `(Global: false)`; exists only for current wire) or globally scoped (created with `(Global: true)`; exists for all wires of that mesh), both parameters `Global` and `Name` are used in combination to identify the correct variable to pop elements from. 

Input field is ignored and the output of this shard is the element which was popped from the sequence passed to it via the `Name` parameter. 

!!! note "See also"
    - [`AppendTo`](../AppendTo)
    - [`Clear`](../Clear)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Erase`](../Erase)
    - [`PopFront`](../PopFront)
    - [`Remove`](../Remove)
   

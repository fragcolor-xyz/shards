`PopFront` drops (removes) the first element of the sequence variable that has been passed to in the `:Name` parameter and makes it available to the next shard as its input.

This shard works only on sequences and the parameters `:Key` and `:Global` are not applicable to it.

Input field is ignored and the output of this shard is the element which was popped from the sequence passed to it via the `:Name` parameter. 

!!! note "See also"
    - [`Pop`](../Pop)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Clear`](../Clear)
    - [`Remove`](../Remove)
    - [`Erase`](../Erase)
   

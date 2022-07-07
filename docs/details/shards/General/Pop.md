`Pop` drops (removes) the last element of the sequence variable that has been passed to in the `:Name` parameter and makes it available to the next shard as its input.


It also makes this dropped element available as its output (for the next shard to consume).

This shard works only on sequences and does not take any input. Its output is the popped element (which was removed from the sequence passed to it via the `:Name` parameter).

!!! note "See also"
    - [`PopFront`](../PopFront)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Clear`](../Clear)
    - [`Remove`](../Remove)
    - [`Erase`](../Erase)
   

`Sort` sorts all the elements of the sequence that has been passed to it in the `Name:` parameter based on the value of the `Desc:` parameter. If `Desc:` is set to true the sequence is sorted in descending order, else it's sorted in the ascending order (which is the default behaviour).

This shard can also take final element order of the sorted sequence and apply that to a joined sequence (passed via the `Join:` parameter). For example, if the element at index-7 moved to index-3 in the main sequence due to sorting then in the joined sequence too the element at index-7 would move to index-3. The movement of all elements in the main sequence (post-sort) would be mirrored in the joined sequence. For this to work both the sequences must have the same length.

!!! note
    Think of this as the Shards equivalent of a relational database inner join. The main sequence and the joined sequence can be thought of as columns from two different tables inner joined over indices equality. So that the changes in elements of one sequence (rows in the first table) can be propogated to the corresponding elements of the joined sequence (corresponding rows in the joined table). 

    In this case the operation is changing of indices (position) of selected elements (selected rows) in one sequence (table) leading to an equivalent change of indices (position) of corresponding elements (connected rows) of the joined sequence (joined table).

The `Key:` parameter can take a shard or group of shards to transform the sequence elements before they're compared for sorting. This transformation doesn't actually change the value of the elements in the final sorted sequence (it's used only for sort comparisons).

`Sort` works only on sequences.

Any input to this shard is ignored and its output is the main sorted sequence.

!!! note "See also"
    - [`Remove`](../Remove)

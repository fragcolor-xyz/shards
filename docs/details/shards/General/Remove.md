`Remove` removes all the elements of the `Name:` parameter sequence that matches the condition laid out in the `Predicate:` parameter.

It can also take these condition-matched indices (from the `From:` sequence) and remove the corresponding elements from a joined sequence (passed via the `Join:` parameter). Remember, `Remove` doesn't apply the `Predicate:` conditions to the joined sequence, but removes corresponding elements from it based on `Predicate:` matched indices of the main sequence. For this to work both the sequences must have the same length.

!!! note
    Think of this as the Shards equivalent of a relational database inner join. The main sequence and the joined sequence can be thought of as columns from two different tables inner joined over indices equality. So that the changes in elements of one sequence (rows in the first table) can be propagated to the corresponding elements of the joined sequence (corresponding rows in the joined table). 

    In this case the operation is deletion of selected elements (selected rows) from one sequence (table) leading to deletion of corresponding elements (connected rows) of the joined sequence (joined table).

The `Predicate:` parameter can take any conditional/logical expression or combination of shards that will result in assertion that can be tested on the sequence elements.

The `Unordered:` parameter can be set to `true` if you need to make the removal process faster, but then the order of the remaining elements in the resulting sequence elements may not be preserved. By default, this order is preserved.

`Remove` works only on sequences.

Any input to this shard is ignored and its output is the main filtered sequence.

!!! note "See also"
    - [`Clear`](../Clear)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Erase`](../Erase)
    - [`Pop`](../Pop)
    - [`PopFront`](../PopFront)
    - [`Sort`](../Sort)

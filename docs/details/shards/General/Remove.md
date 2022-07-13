`Remove` removes all the elements of the `:Name` parameter sequence that match the condition laid out in the the `:Predicate` parameter.

It can also take these condition-matched indices (from the `:From` sequence) and remove the corresponding elements from a joined sequence (passed via the `:Join` parameter). Remember, `Remove` doesn't apply the `:Predicate` conditions to the joined sequence, but removes corresponding elements from it based on `:Predicate` matched indices of the main sequence. For this to work both the sequences must have the same length.

The `:Predicate` parameter can take any conditional/logical expression or combination of shards that will result in assertion that can be tested on the sequence elements.

The `:Unordered` parameter can be set to `true` if you need to make the removal process faster, but then the order of the remaining elements in the resulting sequence elements may not be preserved. By default, this order is preseved.

`Remove` works only on sequences.

Any input to this shard is ignored and its output is the main filtered sequence.

!!! note "See also"
    - [`Pop`](../Pop)
    - [`PopFront`](../PopFront)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Clear`](../Clear)
    - [`Erase`](../Erase)
    - [`Sort`](../Sort)

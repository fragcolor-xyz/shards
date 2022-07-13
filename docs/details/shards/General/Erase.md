`Erase` deletes single or multiple elements (from sequences) and key-value pairs (from tables). 

For a sequence, this shard expects the index (or a sequence of indices in descending order) of the element(s) to be erased, followed by the name of the sequence variable in the `:Name` parameter.

For a table, this shard expects the key (or a sequence of keys) of the key-value pair(s) to be erased, followed by the name of the table variable in the `:Name` parameter. 

`Erase` works only on sequences and tables and the parameter`:Global` is not applicable for this shard.

Any input to this shard is ignored and instead passed through as its output.

!!! note "See also"
    - [`Pop`](../Pop)
    - [`PopFront`](../PopFront)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Drop`](../Drop)
    - [`DropFront`](../DropFront)
    - [`Remove`](../Remove)
    - [`Clear`](../Clear)

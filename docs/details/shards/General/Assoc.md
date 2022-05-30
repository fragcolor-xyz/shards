This shard can be used to update specific member elements within a sequence or a table with new values.

The input sequence identifies which elements are to be updated and their new/ updated values. To achieve this, the member elements of this input sequence are parsed in pairs. The 1st element of each pair gives the index of the target element to update, and the 2nd element of that pair gives the new value for the target element. Due to this, the input sequence must always contain an even number of elements.

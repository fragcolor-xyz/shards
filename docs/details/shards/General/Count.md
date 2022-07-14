`Count` parses the value passed to it in the `:Name` parameter, and returns the count of characters, elements, or key-value pairs depending on whether the data type passed to it was a string, a sequence, or a table. 

If this shard is applied to a number it returns zero as the count. Parameters `:Key` and `:Global` are not applicable to `Count`. 

Input field is ignored and the output of this shard is the count value in `Int` type.

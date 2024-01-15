`Count` parses the value passed to it in the `Name:` parameter, and returns the count of characters, elements, or key-value pairs depending on whether the data type passed to it was a string, a sequence, or a table. 

If this shard is applied to a number it returns zero as the count. 

This shard works on both sequences and tables. Parameter `Key:` applies only to tables.

Since variables may be locally scoped (created with `(Global: false)`; exists only for current wire) or globally scoped (created with `(Global: true)`; exists for all wires of that mesh), both parameters `Global:` and `Name:` are used in combination to identify the correct variable to count. 

Input field is ignored and the output of this shard is the count value in `Int` type.

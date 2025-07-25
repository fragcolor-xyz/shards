If the variable to count is a table, the `Key` parameter identifies which key to target. The value in the key specified still needs to be a countable value.

Since variables may be locally scoped (created with `Global: false`; exists only for current wire) or globally scoped (created with `Global: true`; exists for all wires of that mesh), both parameters `Global` and `Name` are used in combination to identify the correct variable to count.

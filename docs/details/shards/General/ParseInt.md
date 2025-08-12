The value passed into the `Base` parameter determines which characters are valid. The default base for this shard is 10.

The value passed into the `Base` parameter must be between 2 and 36.

For bases beyond 10, alphabetical characters are given a value and it is not case-sensitive.

Some common bases include:
  - 2 (binary) - only valid characters are 0 and 1.
  - 8 (octal) - only valid characters are 0-7.
  - 10 (decimal) - Standard base, valid characters are 0-9. (default)
  - 16 (hexadecimal) - only valid characters are 0-9, a-f, and A-F.
  - 36 (base-36) - only valid characters are 0-9, a-z, and A-Z.
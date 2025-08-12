If an integer was encoded with ToLEB128 with Signed set to true, it can be decoded with FromLEB128 with Signed set to true and vice versa.

Consider the following example: `-1000 | ToLEB128(Signed: false) | FromLEB128(Signed: true)`
  This will throw an error because the input was not encoded with Signed set to true.

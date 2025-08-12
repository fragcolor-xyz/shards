This shard calculates the square root of a number faster than `Math.Sqrt`, but is less accurate. For most everyday numbers, the difference is negligible. The difference is more pronounced for very large numbers, very small numbers, or numbers very close to perfect squares.

If a sequence of floats was provided as input, the shard will calculate the square root of each element in the sequence and output a sequence.

If a sequence of vectors was provided as input, the shard will calculate the square root of each component in each vector in the sequence and output a sequence of vectors where each component in each vector is the square root of the corresponding component in the input sequence.
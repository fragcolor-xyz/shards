If the inputs with fractional parts that are greater than 0.5, the output will be rounded up.
If the inputs with fractional parts that are less than 0.5, the output will be rounded down.
If the inputs with fractional parts that are equal to 0.5, the output will be rounded to the nearest even number.

If a sequence of floats was provided as input, the shard will round each element in the sequence and output a sequence.

If a sequence of vectors was provided as input, the shard will round each component in each vector in the sequence and output a sequence of vectors with each component in each vector rounded.
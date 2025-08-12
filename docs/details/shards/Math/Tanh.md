Tanh is not a periodic function. It does not repeat its values.

Tanh is bounded and will always return a value between -1 and 1 but approaches 1 as the input value grows positively and -1 as the input value grows negatively.

If a sequence of floats was provided as input, the shard will calculate the hyperbolic tangent of each element in the sequence and output a sequence.

If a sequence of vectors was provided as input, the shard will calculate the hyperbolic tangent of each component in each vector in the sequence and output a sequence of vectors where each component in each vector is the hyperbolic tangent of the corresponding component in the input sequence.
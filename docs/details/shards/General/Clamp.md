This shard can accept integer or float vectors as input.

The input type and the type provided in the `Min` and `Max` parameters must be the same. (eg. if the input is a vector, the `Min` and `Max` parameters must be a vector of the same type)

The input vector and the `Min` and `Max` vector must be of the same length.

If the shard is comparing vectors, it will clamp each corresponding element in the input vector with the corresponding element in the `Min` and `Max`.
Consider the following example:
``` shards
@i2(2 5)
  Clamp(Min: @i2(3 3) Max: @i2(4 4))
  Log("Result")
```
The result will be `@i2(3 4)`. The first element in the input vector is 2, which is less than the first element in the `Min` vector (3), so it is clamped to 3. The second element in the input vector is 5, which is greater than the second element in the `Max` vector (4), so it is clamped to 4. It will process the `Min` parameter first and then the `Max` parameter.


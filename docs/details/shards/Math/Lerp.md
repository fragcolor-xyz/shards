The value in the `First` parameter and the `Second` parameter must be of the same type.

If the integers are provided in the `First` and `Second` parameters, the output of the shard will be an integer rounded down to the nearest integer.

If vectors are provided in the `First` and `Second` parameters, The first element in the `First` vector will be linearly interpolated to the first element in the `Second` vector, the second element in the `First` vector will be linearly interpolated to the second element in the `Second` vector, and so on.

Do note that the output of the shard is not automatically written to the value in the `First` parameter.
Consider the following example:
``` shards
@wire(lerp {
  Once({
    0 >= x
  })
  
  0.1
  Math.Lerp(First: x Second: 100)
  Log("Result")
} Looped: true)
```
The output will always be `10` regardless of how many times the Math.Lerp shard is called. For the output value to progress, you must write the output back to `x`. Like so:
``` shards
@wire(lerp {
  Once({
    0 >= x
  })
  
  0.1
  Math.Lerp(First: x Second: 100)
  > x
  Log("Result")
} Looped: true)
```
Now the first time `Math.Ler` is called the output will be 10, then 20 and so on.
The `Params` parameter can accept a table with keys representing the shader parameters for the material and their respective values. For example:
  ``` shards
  GFX.Material(Params: {
  baseColor: @f4(1.0 0.5 0.2 1.0)
  roughness: 0.7
  metallic: 0.2
  emissive: @f3(0.1 0.1 0.1)
  normalScale: 1.0
    aoStrength: 0.5
  })
  ```

The `Features` parameter can accept a sequence of feature objects, created either with the `GFX.Feature` or `GFX.BuiltinFeature` shard.
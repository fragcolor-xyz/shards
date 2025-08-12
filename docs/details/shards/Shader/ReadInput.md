This can be used in the `Shader` parameter of `GFX.Feature` or the `EntryPoint` parameter of `GFX.EffectPass`.

The functions differently when used in the Vertex stage and the Fragment stage of the entry point.
  - When used in the vertex stage, this shard will read the value of the vertex attribute specified in the `Name` parameter.
  - When used in the fragment stage, it will read the specified interpolated vertex stage output that is supplied to the pixel. 
  
This shard is able to read a vertex attribute even if it was not explicitly written and output in the vertex stage using `Shader.WriteOutput`.
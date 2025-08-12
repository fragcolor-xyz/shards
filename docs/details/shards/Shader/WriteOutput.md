This can be used in the `Shader` parameter of `GFX.Feature` or the `EntryPoint` parameter of `GFX.EffectPass`.

The functions differently when used in the Vertex stage and the Fragment stage of the entry point.
  - When used in the Vertex stage, will write or create a new shader output and supply it to the fragment stage.
  - When used in the Fragment stage, it will write the value supplied as input to the shard, to one of the outputs attached to the render pass.
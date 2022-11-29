A Feature can be thought of as an extension or plugin to the rendering logic.

A Feature may have various components:

- Blend State (Alpha, Additive, ...)
- Depth Testing
- Vertex/Fragment shaders

For a more extensive description about shaders, see [Shaders](../Shader/index.md).

Features can be used in various locations:
- Attached to a [GFX.DrawablePass](DrawablePass.md)
- Attached to a [GFX.EffectPass](EffectPass.md)

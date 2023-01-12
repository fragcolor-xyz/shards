This shard generates features with preset behavior, the behavior depends on the [Id](../../../enums/BuiltinFeatureId).

## Built-in Feature Ids

### Transform

This Feature implements basic world/view/projection transform.

Here are the entry points that are defined and can be added as dependencies by your own features.

#### `initLocalPosition` (vertex)

Sets the `localPosition` (float4) global from the vertex position.

#### `initScreenPosition` (vertex)

Sets the `screenPosition` global (float4) to the transformed vertex position (`=proj*view*world*<vertex>`).

#### `writePosition` (vertex)

Writes `screenPosition` global to `position` output.

#### `initWorldNormal` (vertex)

Transforms the object normal and writes the transformed result into the `worldNormal` global (float3).
If the mesh doesn't have vertex normals, the normal will be `(0.0, 0.0, 1.0)`.

#### `writeNormal` (vertex)

Writes `worldNormal` global into `worldNormal` output.

### BaseColor

This Feature adds a per-object shader parameter with the name `baseColor` and a texture parameter with the same name.

Here are the entry points that are defined and can be added as dependencies by your own features.

#### `initColor` (vertex)

This vertex shader entry point sets up the `color` global (float4) with the vertex color from the mesh if it has been set. If the mesh doesn't have a vertex color, it'll be white with alpha 1.0.

#### `writeColor` (vertex)

Writes `color` global to `color` output.

#### `readColor` (fragment)

Reads `color` input into `color` global (float4).

#### `textureColor` (fragment)

Reads the texture color from the `baseColor` texture and multiplies it with the current `color` global. It does nothing if the texture does not exist.

#### `writeColor` (fragment)

Writes `color` global to `color` output. `color` is the default name for the main color output from the fragment shader.

#### VertexColorFromNormal

This Feature outputs the mesh normal as a per-vertex color output.

Use for debugging.

### Wireframe

This Feature changes how objects are rendered so that their edges are highlighted. Very basic, with a fixed edge width and color.

Use for debugging.


### Velocity

This Feature outputs per-object velocity into a `velocity` output and `velocity` global. For usage with effects that require a velocity buffer such as motion blur or temporal anti-aliasing.

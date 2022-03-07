# Graphics framework

## Windowing/Context

`Window` wraps around SDL to deal with input and platform specific window stuff\
`Context` wraps a bgfx context and some persistent states/caching

## Frame renderer

`frame.hpp/cpp` Provides a structure for the duration of a frame that allocates bgfx views and keeps them in a stack along with viewport/camera matrix info

## Materials

Materials are the core of the graphics framework, they abstract away the shader permutations\
`material.hpp/cpp` Contains the structure that maps to the `Material` block containing parameters, flags and shader code\
the basic structure looks like:

```cpp
struct Material {
	MaterialStaticFlags::Type flags = MaterialStaticFlags::None;
	std::unordered_map<std::string, float4> vectorParameters;
	std::unordered_map<std::string, MaterialTextureSlot> textureSlots;
	std::vector<uint32_t> mrtOutputs;
	std::string vertexCode;
	std::string pixelCode;
};
```

## Material usage

Whenever a material is used for a draw command a `MaterialUsageContext` is used to collect compile-time and runtime shader parameters based on the current usage of the material.

Storage is linked to the unique material/mesh combination (per `Draw` block)

Examples of parameters collected:

- Mesh attributes (does it have normal/tangent/color attributes?)
- Skeletal animation (do we need to emit animation shader code?)
- Forward lighting parameters (how many point/dir/env lights?, specify lighting textures, etc.)

After collecting the parameters this object is responsible for:

- composing & compiling shaders if needed
- Assigning texture parameters to registers/units
- Assigning textures to uniforms
- Assigning float parameters to uniforms
- Setting the pipeline state (blend/clipping/etc.)

## Shader building

`material_shader.hpp/cpp` Contains logic for composing shader code out of a given material and usage parameters.\
currently feature-specific defines are set within this source file based on the `StaticMaterialOptions` in the MaterialUsageContext

ps_entry/vs_entry are ubershader entry points for materials. custom shader code is appended at the end and manipulates the fields inside MaterialInfo

## Features

To create independent units of code that can modify shader generation and inject parameters the code is split up into into features.

An example of what this allows is give the EnvironmentLights feature a precompute pass that captures the scene while excluding itself from being applied to draw objects

Similarly this can be applied to shadow map rendering, where the scene is draw again with reduced feature set (no lights)

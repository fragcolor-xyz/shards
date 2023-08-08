#ifndef E3C5828A_F272_4897_B1E6_1A4364B4D5D3
#define E3C5828A_F272_4897_B1E6_1A4364B4D5D3

#include <string>
#include "shader/uniforms.hpp"
#include "shader/generator.hpp"
#include "shader/entry_point.hpp"
#include "shader/textures.hpp"
#include "renderer_types.hpp"

namespace gfx {

// Use configurable pipeline options
struct BuildPipelineOptions {
  // If this is set, no features from drawables will be included in rendering
  // usefull for debug passes or other special cases
  bool ignoreDrawableFeatures{};

  template <typename T> void getPipelineHash(T &hasher) const { hasher(ignoreDrawableFeatures); }
};

// Describes a buffer binding being built
struct BufferBindingBuilder {
  // The bind group to add this binding to
  BindGroupId bindGroupId = BindGroupId::Draw;
  // Identifier of the binding
  std::string name;
  // The builder that is being used to define the binding's structure layout
  shader::UniformBufferLayoutBuilder layoutBuilder;
  // Set to true if the binding is not used by any shader function
  bool unused = false;
  // The type of storage to use
  shader::BufferType bufferType = shader::BufferType::Uniform;
  // Specifies if the binding is a single structure or a fixed/dynamic array of structures
  shader::Dimension dimension;
  // Specifies that this binding accepts a dynamic offset during the setting of the bind group
  bool hasDynamicOffset = false;
  // Bind group index
  size_t bindGroup{};
  // Binding index in the bind group
  size_t binding{};
};

struct PipelineBuilder {
  // The built pipeline
  detail::CachedPipeline &output;

  detail::RenderTargetLayout renderTargetLayout;

  // First drawable that is grouped into this pipeline
  const IDrawable &firstDrawable;

  MeshFormat meshFormat;

  BuildPipelineOptions options;

  // Indicates that rendering happens with an inverted camera matrix
  // winding order of geometry should be flipped to compensate
  bool isRenderingFlipped{};

  // All features that apply to this pipeline
  std::vector<const Feature *> features;

  // Descriptions of the buffers that will be bound to this pipeline
  std::vector<BufferBindingBuilder> bufferBindings;

  // Descriptions of the textures that will be bound to this pipeline
  shader::TextureBindingLayoutBuilder textureBindings;

  // Description of material texture parameters
  // During collectTextureBindings these values are collected into textureBindings
  std::map<std::string, TextureParameter> materialTextureBindings;

  // Cache variables
  std::vector<const shader::EntryPoint *> shaderEntryPoints;
  std::vector<BufferBindingBuilder *> drawBindings;
  std::vector<BufferBindingBuilder *> viewBindings;
  shader::Generator generator;

  PipelineBuilder(detail::CachedPipeline &output, const detail::RenderTargetLayout &rtl, const IDrawable &firstDrawable)
      : output(output), renderTargetLayout(rtl), firstDrawable(firstDrawable) {}

  BufferBindingBuilder &getOrCreateBufferBinding(std::string &&name);
  void build(WGPUDevice device, const WGPULimits &deviceLimits);

  static size_t getViewBindGroupIndex();
  static size_t getDrawBindGroupIndex();

private:
  void collectShaderEntryPoints();
  void collectTextureBindings();
  void buildPipelineLayout(WGPUDevice device, const WGPULimits &deviceLimits);

  // Setup buffer/texture definitions in the shader generator
  void setupShaderDefinitions(const WGPULimits &deviceLimits, bool final);

  void setupShaderOutputFields();

  shader::GeneratorOutput generateShader();

  void finalize(WGPUDevice device);

  // Strip unused fields from bindings
  void optimizeBufferLayouts(const shader::IndexedBindings &indexedShaderDindings);
};

// Low-level entry-point for modifying render pipelines
struct IPipelineModifier {
  virtual ~IPipelineModifier() = default;
  virtual void buildPipeline(PipelineBuilder &builder, const BuildPipelineOptions &options){};
};

} // namespace gfx

#endif /* E3C5828A_F272_4897_B1E6_1A4364B4D5D3 */

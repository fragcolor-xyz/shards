#ifndef E3C5828A_F272_4897_B1E6_1A4364B4D5D3
#define E3C5828A_F272_4897_B1E6_1A4364B4D5D3

#include <string>
#include "shader/uniforms.hpp"
#include "shader/generator.hpp"
#include "shader/entry_point.hpp"
#include "shader/textures.hpp"
#include "renderer_types.hpp"

namespace gfx {

enum class BindingFrequency {
  View,
  Draw,
};

// Describes a buffer binding being built
struct BufferBindingBuilder {
  BindingFrequency frequency = BindingFrequency::Draw;
  std::string name;
  shader::UniformBufferLayoutBuilder layoutBuilder;
  bool unused = false;
  shader::BufferType bufferType = shader::BufferType::Uniform;
  bool hasDynamicOffset = false;
  size_t bindGroup{};
  size_t binding{};
};

struct PipelineBuilder {
  // Output
  detail::CachedPipeline &output;

  detail::RenderTargetLayout renderTargetLayout;

  // First drawable that is grouped into this pipeline
  const IDrawable &firstDrawable;

  MeshFormat meshFormat;

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
  virtual void buildPipeline(PipelineBuilder &builder){};
};

} // namespace gfx

#endif /* E3C5828A_F272_4897_B1E6_1A4364B4D5D3 */

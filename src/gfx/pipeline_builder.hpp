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
  BindingFrequency frequency;
  std::string name;
  shader::UniformBufferLayoutBuilder layoutBuilder;
  bool unused = false;
  size_t bindGroup;
  size_t binding;
};

struct PipelineBuilder {
  detail::CachedPipeline &cachedPipeline;
  std::vector<BufferBindingBuilder> bufferBindings;
  shader::TextureBindingLayoutBuilder textureBindings;
  std::vector<const shader::EntryPoint *> shaderEntryPoints;
  std::vector<BufferBindingBuilder *> drawBindings;
  std::vector<BufferBindingBuilder *> viewBindings;
  std::vector<const Feature*> features;
  shader::Generator generator;
  const IDrawable &firstDrawable;

  PipelineBuilder(detail::CachedPipeline &cachedPipeline, const IDrawable &firstDrawable)
      : cachedPipeline(cachedPipeline), firstDrawable(firstDrawable) {}

  BufferBindingBuilder &getOrCreateBufferBinding(std::string &&name);
  WGPURenderPipeline build(WGPUDevice device, const WGPULimits &deviceLimits);

  static size_t getViewBindGroupIndex();
  static size_t getDrawBindGroupIndex();

private:
  void collectShaderEntryPoints();
  void collectTextureBindings();
  void buildBufferBindingLayouts();
  void buildPipelineLayout(WGPUDevice device, const WGPULimits &deviceLimits);

  // Setup buffer/texture definitions in the shader generator
  void setupShaderDefinitions(const WGPULimits &deviceLimits, bool final);

  void setupShaderOutputFields();

  shader::GeneratorOutput generateShader();

  WGPURenderPipeline finalize(WGPUDevice device);

  // Strip unused fields from bindings
  void optimizeBufferLayouts(const shader::IndexedBindings &indexedShaderDindings);
};

// Low-level entry-point for modifying render pipelines
struct IPipelineModifier {
  virtual ~IPipelineModifier() = default;
  virtual void buildPipeline(PipelineBuilder &builder, const IDrawable &referenceDrawable){};
};

} // namespace gfx

#endif /* E3C5828A_F272_4897_B1E6_1A4364B4D5D3 */

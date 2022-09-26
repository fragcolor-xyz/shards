#include "pipeline_builder.hpp"
#include "shader/uniforms.hpp"
#include "log.hpp"
#include "shader/textures.hpp"
#include "shader/generator.hpp"
#include "shader/blocks.hpp"

using namespace gfx::detail;
using namespace gfx::shader;
namespace gfx {

static auto logger = getLogger();

BufferBindingBuilder &PipelineBuilder::getOrCreateBufferBinding(std::string &&name) {
  auto it = std::find_if(bufferBindings.begin(), bufferBindings.end(),
                         [&](const BufferBindingBuilder &builder) { return builder.name == name; });
  if (it != bufferBindings.end())
    return *it;

  return bufferBindings.emplace_back(BufferBindingBuilder{
      .name = std::move(name),
  });
}

void buildBindGroup() {}

void PipelineBuilder::collectShaderEntryPoints() {
  for (auto &feature : cachedPipeline.features) {
    for (auto &entryPoint : feature->shaderEntryPoints) {
      shaderEntryPoints.push_back(&entryPoint);
    }
  }

  static const std::vector<EntryPoint> &builtinEntryPoints = []() -> const std::vector<EntryPoint> & {
    static std::vector<EntryPoint> builtin;
    builtin.emplace_back("interpolate", ProgrammableGraphicsStage::Vertex, blocks::DefaultInterpolation());
    return builtin;
  }();

  for (auto &builtinEntryPoint : builtinEntryPoints) {
    shaderEntryPoints.push_back(&builtinEntryPoint);
  }
}

void PipelineBuilder::optimizeBufferLayouts(const shader::IndexedBindings &indexedShaderDindings) {
  auto &usedBufferBindings = indexedShaderDindings.bufferBindings;
  for (auto &bufferBinding : bufferBindings) {
    auto it = std::find_if(usedBufferBindings.begin(), usedBufferBindings.end(),
                           [&](auto &binding) { return binding.name == bufferBinding.name; });
    if (it == usedBufferBindings.end()) {
      bufferBinding.unused = true;
      continue;
    }

    auto &accessedFields = it->accessedFields;
    auto &layoutBuilder = bufferBinding.layoutBuilder;
    layoutBuilder.optimize([&](const std::string &fieldName, UniformLayout &fieldLayout) {
      auto it = accessedFields.find(fieldName);
      return it != accessedFields.end();
    });
  }
}

WGPURenderPipeline PipelineBuilder::build(WGPUDevice device, const WGPULimits &deviceLimits) {
  auto &viewBinding = getOrCreateBufferBinding("view");
  viewBinding.frequency = BindingFrequency::View;
  auto &viewLayoutBuilder = viewBinding.layoutBuilder;
  viewLayoutBuilder.push("view", FieldTypes::Float4x4);
  viewLayoutBuilder.push("proj", FieldTypes::Float4x4);
  viewLayoutBuilder.push("invView", FieldTypes::Float4x4);
  viewLayoutBuilder.push("invProj", FieldTypes::Float4x4);
  viewLayoutBuilder.push("viewport", FieldTypes::Float4);

  auto &objectBinding = getOrCreateBufferBinding("object");
  objectBinding.frequency = BindingFrequency::Draw;
  auto &objectLayoutBuilder = objectBinding.layoutBuilder;
  objectLayoutBuilder.push("world", FieldTypes::Float4x4);
  objectLayoutBuilder.push("worldInvTrans", FieldTypes::Float4x4);

  for (const Feature *feature : cachedPipeline.features) {
    // Object parameters
    for (auto &param : feature->shaderParams) {
      objectLayoutBuilder.push(param.name, param.type);
    }

    // Apply modifiers
    if (feature->pipelineModifier) {
      feature->pipelineModifier->buildPipeline(*this);
    }
  }

  // Populate shaderEntryPoints from features & builtin entry points
  collectShaderEntryPoints();

  // Scan shader for accessed buffer/texture bindings
  shader::IndexedBindings indexedShaderBindings = generator.indexBindings(shaderEntryPoints);
  indexedShaderBindings.dump();

  // Remove unused fields from buffer layouts
  optimizeBufferLayouts(indexedShaderBindings);

  // Sort bindings into per draw/view
  for (auto &binding : bufferBindings) {
    switch (binding.frequency) {
    case BindingFrequency::Draw:
      drawBindings.push_back(&binding);
      break;
    case BindingFrequency::View:
      viewBindings.push_back(&binding);
      break;
    }
  }

  // Collect texture parameters from features/materials
  collectTextureBindings();

  buildPipelineLayout(device, deviceLimits);

  // Actually create the pipeline object
  return finalize(device);
}

static void describeBindings(const std::vector<BufferBindingBuilder *> &bindings,
                             std::vector<WGPUBindGroupLayoutEntry> &outEntries,
                             std::vector<shader::BufferBinding> &outShaderBindings, size_t &bindingCounter, size_t bindGroup,
                             const WGPULimits &deviceLimits, BindingFrequency freq) {
  for (auto &binding : bindings) {
    binding->bindGroup = bindGroup;
    binding->binding = bindingCounter++;

    auto &shaderBinding = outShaderBindings.emplace_back(shader::BufferBinding{
        .bindGroup = binding->bindGroup,
        .binding = binding->binding,
        .name = binding->name,
        .layout = binding->layoutBuilder.finalize(),
    });

    if (freq == BindingFrequency::Draw) {
      // Align the struct to storage buffer offset requriements
      UniformBufferLayout &layout = shaderBinding.layout;
      layout.maxAlignment = alignTo(layout.maxAlignment, deviceLimits.minStorageBufferOffsetAlignment);
    }

    auto &bglEntry = outEntries.emplace_back();
    bglEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
    bglEntry.binding = shaderBinding.binding;

    auto &bufferBinding = bglEntry.buffer;
    bufferBinding.hasDynamicOffset = false;

    switch (freq) {
    case BindingFrequency::Draw:
      shaderBinding.type = BufferType::Storage;
      bufferBinding.type = WGPUBufferBindingType_ReadOnlyStorage;
      shaderBinding.indexedPerInstance = true;
      break;
    case BindingFrequency::View:
      shaderBinding.type = BufferType::Uniform;
      bufferBinding.type = WGPUBufferBindingType_Uniform;
      break;
    }
  }
}

size_t PipelineBuilder::getViewBindGroupIndex() { return 1; }
size_t PipelineBuilder::getDrawBindGroupIndex() { return 0; }

void PipelineBuilder::buildPipelineLayout(WGPUDevice device, const WGPULimits &deviceLimits) {
  assert(!cachedPipeline.pipelineLayout);
  assert(cachedPipeline.bindGroupLayouts.empty());

  // Create per-draw bind group (0)
  size_t bindGroup = getDrawBindGroupIndex();
  size_t bindingCounter = 0;
  std::vector<WGPUBindGroupLayoutEntry> bindGroupLayoutEntries;

  describeBindings(drawBindings, bindGroupLayoutEntries, generator.bufferBindings, bindingCounter, bindGroup, deviceLimits,
                   BindingFrequency::Draw);

  // Append texture bindings to per-draw bind group
  cachedPipeline.textureBindingLayout = textureBindings.finalize(bindingCounter, &bindingCounter);
  auto &textureBindingLayout = cachedPipeline.textureBindingLayout;

  // Interleaved Texture&Sampler bindings (1 each per texture slot)
  for (auto &desc : textureBindingLayout.bindings) {
    WGPUBindGroupLayoutEntry &textureBinding = bindGroupLayoutEntries.emplace_back();
    textureBinding.binding = desc.binding;
    textureBinding.visibility = WGPUShaderStage_Fragment;
    textureBinding.texture.multisampled = false;
    textureBinding.texture.sampleType = WGPUTextureSampleType_Float;
    textureBinding.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry &samplerBinding = bindGroupLayoutEntries.emplace_back();
    samplerBinding.binding = desc.defaultSamplerBinding;
    samplerBinding.visibility = WGPUShaderStage_Fragment;
    samplerBinding.sampler.type = WGPUSamplerBindingType_Filtering;
  }

  // Pass to shader generator
  generator.textureBindingLayout = textureBindingLayout;
  generator.textureBindGroup = bindGroup;

  WGPUBindGroupLayoutDescriptor desc{
      .label = "per-draw",
      .entryCount = uint32_t(bindGroupLayoutEntries.size()),
      .entries = bindGroupLayoutEntries.data(),
  };
  cachedPipeline.bindGroupLayouts.emplace_back(wgpuDeviceCreateBindGroupLayout(device, &desc));

  // Create per-view bind group (1)
  bindGroup = getViewBindGroupIndex();
  bindingCounter = 0;
  bindGroupLayoutEntries.clear();

  describeBindings(viewBindings, bindGroupLayoutEntries, generator.bufferBindings, bindingCounter, bindGroup, deviceLimits,
                   BindingFrequency::View);

  desc = WGPUBindGroupLayoutDescriptor{
      .label = "per-view",
      .entryCount = uint32_t(bindGroupLayoutEntries.size()),
      .entries = bindGroupLayoutEntries.data(),
  };
  cachedPipeline.bindGroupLayouts.emplace_back(wgpuDeviceCreateBindGroupLayout(device, &desc));

  WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
  pipelineLayoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout *>(cachedPipeline.bindGroupLayouts.data());
  pipelineLayoutDesc.bindGroupLayoutCount = cachedPipeline.bindGroupLayouts.size();
  cachedPipeline.pipelineLayout.reset(wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc));

  // Store run-time binding info
  cachedPipeline.drawBufferBindings.reserve(drawBindings.size());
  cachedPipeline.viewBuffersBindings.reserve(viewBindings.size());
  for (auto &binding : generator.bufferBindings) {
    std::vector<gfx::detail::BufferBinding> *outArray{};
    if (binding.bindGroup == getDrawBindGroupIndex()) {
      outArray = &cachedPipeline.drawBufferBindings;
    } else {
      outArray = &cachedPipeline.viewBuffersBindings;
    }

    auto &outBinding = (*outArray).emplace_back();
    outBinding.layout = binding.layout;
    outBinding.index = binding.binding;
  }
}

FeaturePipelineState computePipelineState(const std::vector<const Feature *> &features) {
  FeaturePipelineState state{};
  for (const Feature *feature : features) {
    state = state.combine(feature->state);
  }
  return state;
}

shader::GeneratorOutput PipelineBuilder::generateShader() {
  using namespace gfx::shader::blocks;

  generator.meshFormat = cachedPipeline.meshFormat;

  FieldType colorFieldType(ShaderFieldBaseType::Float32, 4);
  generator.outputFields.emplace_back("color", colorFieldType);

  return generator.build(shaderEntryPoints);
}

WGPURenderPipeline PipelineBuilder::finalize(WGPUDevice device) {
  FeaturePipelineState pipelineState = computePipelineState(cachedPipeline.features);

  shader::GeneratorOutput generatorOutput = generateShader();
  if (generatorOutput.errors.size() > 0) {
    shader::GeneratorOutput::dumpErrors(generatorOutput);
    assert(false);
  }

  WGPUShaderModuleDescriptor moduleDesc = {};
  WGPUShaderModuleWGSLDescriptor wgslModuleDesc = {};
  moduleDesc.label = "pipeline";
  moduleDesc.nextInChain = &wgslModuleDesc.chain;

  wgslModuleDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  wgpuShaderModuleWGSLDescriptorSetCode(wgslModuleDesc, generatorOutput.wgslSource.c_str());
  SPDLOG_LOGGER_DEBUG(logger, "Generated WGSL:\n {}", generatorOutput.wgslSource);

  cachedPipeline.shaderModule.reset(wgpuDeviceCreateShaderModule(device, &moduleDesc));
  assert(cachedPipeline.shaderModule);

  WGPURenderPipelineDescriptor desc = {};
  desc.layout = cachedPipeline.pipelineLayout;

  VertexStateBuilder vertStateBuilder;
  vertStateBuilder.build(desc.vertex, cachedPipeline);

  WGPUFragmentState fragmentState = {};
  fragmentState.entryPoint = "fragment_main";
  fragmentState.module = cachedPipeline.shaderModule;

  // Shared blend state for all color targets
  std::optional<WGPUBlendState> blendState{};
  if (pipelineState.blend.has_value()) {
    blendState = pipelineState.blend.value();
  }

  // Color targets
  std::vector<WGPUColorTargetState> colorTargets;
  for (auto &target : cachedPipeline.renderTargetLayout.colorTargets) {
    WGPUColorTargetState &colorTarget = colorTargets.emplace_back();
    colorTarget.format = target.format;
    colorTarget.writeMask = pipelineState.colorWrite.value_or(WGPUColorWriteMask_All);
    colorTarget.blend = blendState.has_value() ? &blendState.value() : nullptr;
  }
  fragmentState.targets = colorTargets.data();
  fragmentState.targetCount = colorTargets.size();

  // Depth target
  WGPUDepthStencilState depthStencilState{};
  if (cachedPipeline.renderTargetLayout.depthTarget) {
    auto &target = cachedPipeline.renderTargetLayout.depthTarget.value();

    depthStencilState.format = target.format;
    depthStencilState.depthWriteEnabled = pipelineState.depthWrite.value_or(true);
    depthStencilState.depthCompare = pipelineState.depthCompare.value_or(WGPUCompareFunction_Less);
    depthStencilState.stencilBack.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    desc.depthStencil = &depthStencilState;
  }

  desc.fragment = &fragmentState;

  desc.multisample.count = 1;
  desc.multisample.mask = ~0;

  desc.primitive.frontFace = cachedPipeline.meshFormat.windingOrder == WindingOrder::CCW ? WGPUFrontFace_CCW : WGPUFrontFace_CW;
  if (pipelineState.culling.value_or(true)) {
    desc.primitive.cullMode = pipelineState.flipFrontFace.value_or(false) ? WGPUCullMode_Front : WGPUCullMode_Back;
  } else {
    desc.primitive.cullMode = WGPUCullMode_None;
  }

  switch (cachedPipeline.meshFormat.primitiveType) {
  case PrimitiveType::TriangleList:
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    break;
  case PrimitiveType::TriangleStrip:
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
    desc.primitive.stripIndexFormat = getWGPUIndexFormat(cachedPipeline.meshFormat.indexFormat);
    break;
  }

  return wgpuDeviceCreateRenderPipeline(device, &desc);
}

void PipelineBuilder::collectTextureBindings() {
  for (auto &feature : cachedPipeline.features) {
    for (auto &textureParam : feature->textureParams) {
      textureBindings.addOrUpdateSlot(textureParam.name, 0);
    }
  }

  // Update default texture coordinates based on material
  for (auto &pair : cachedPipeline.materialTextureBindings) {
    textureBindings.tryUpdateSlot(pair.first, pair.second.defaultTexcoordBinding);
  }
}

void PipelineBuilder::buildBufferBindingLayouts() {}

} // namespace gfx

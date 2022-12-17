#include "pipeline_builder.hpp"
#include "shader/uniforms.hpp"
#include "log.hpp"
#include "shader/textures.hpp"
#include "shader/generator.hpp"
#include "shader/blocks.hpp"
#include "shader/wgsl_mapping.hpp"
#include "shader/log.hpp"

using namespace gfx::detail;
using namespace gfx::shader;
namespace gfx {

static auto logger = getLogger();

void alignBufferLayout(size_t &inOutSize, const BufferBindingBuilder &builder, const WGPULimits &deviceLimits) {
  if (builder.bufferType == BufferType::Storage)
    inOutSize = alignTo(inOutSize, deviceLimits.minStorageBufferOffsetAlignment);
}

static void describeShaderBindings(const std::vector<BufferBindingBuilder *> &bindings, bool isFinal,
                                   std::vector<shader::BufferBinding> &outShaderBindings, size_t &bindingCounter,
                                   size_t bindGroup, const WGPULimits &deviceLimits, BindingFrequency freq) {
  for (auto &builder : bindings) {
    builder->bindGroup = bindGroup;
    builder->binding = bindingCounter++;

    auto &shaderBinding = outShaderBindings.emplace_back(shader::BufferBinding{
        .bindGroup = builder->bindGroup,
        .binding = builder->binding,
        .name = builder->name,
        .layout = !isFinal ? builder->layoutBuilder.getCurrentFinalLayout() : builder->layoutBuilder.finalize(),
    });

    // Align the struct to storage buffer offset requriements
    UniformBufferLayout &layout = shaderBinding.layout;
    alignBufferLayout(layout.maxAlignment, *builder, deviceLimits);

    shaderBinding.type = builder->bufferType;
    if (freq == BindingFrequency::Draw)
      shaderBinding.indexedPerInstance = true;
  }
}

static void describeBindGroup(const std::vector<BufferBindingBuilder *> &builders, size_t bindGroupIndex,
                              std::vector<WGPUBindGroupLayoutEntry> &outEntries, const WGPULimits &limits) {
  for (auto &builder : builders) {
    if (builder->bindGroup != bindGroupIndex)
      continue;

    auto &bglEntry = outEntries.emplace_back();
    bglEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
    bglEntry.binding = builder->binding;

    auto &bufferBinding = bglEntry.buffer;
    bufferBinding.hasDynamicOffset = builder->hasDynamicOffset;

    switch (builder->bufferType) {
    case shader::BufferType::Uniform:
      bufferBinding.type = WGPUBufferBindingType_Uniform;
      break;
    case shader::BufferType::Storage:
      bufferBinding.type = WGPUBufferBindingType_ReadOnlyStorage;
      break;
    }

    bufferBinding.minBindingSize = builder->layoutBuilder.getCurrentFinalLayout().size;
    alignBufferLayout(bufferBinding.minBindingSize, *builder, limits);
  }
}

static FeaturePipelineState computePipelineState(const std::vector<const Feature *> &features) {
  FeaturePipelineState state{};
  for (const Feature *feature : features) {
    state = state.combine(feature->state);
  }
  return state;
}

static void buildBaseDrawParameters(ParameterStorage &params, const std::vector<const Feature *> &features) {
  for (const Feature *feature : features) {
    for (auto &param : feature->shaderParams) {
      params.setParam(param.name, param.defaultValue);
    }
  }
}

BufferBindingBuilder &PipelineBuilder::getOrCreateBufferBinding(std::string &&name) {
  auto it = std::find_if(bufferBindings.begin(), bufferBindings.end(),
                         [&](const BufferBindingBuilder &builder) { return builder.name == name; });
  if (it != bufferBindings.end())
    return *it;

  return bufferBindings.emplace_back(BufferBindingBuilder{
      .name = std::move(name),
  });
}

void PipelineBuilder::collectShaderEntryPoints() {
  for (const Feature *feature : features) {
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
    layoutBuilder.optimize([&](const std::string &fieldName, const UniformLayout &fieldLayout) {
      auto it = accessedFields.find(fieldName);
      return it != accessedFields.end();
    });
  }
}

void PipelineBuilder::build(WGPUDevice device, const WGPULimits &deviceLimits) {
  auto &viewBinding = getOrCreateBufferBinding("view");
  viewBinding.frequency = BindingFrequency::View;

  auto &objectBinding = getOrCreateBufferBinding("object");
  objectBinding.frequency = BindingFrequency::Draw;

  auto &objectLayoutBuilder = objectBinding.layoutBuilder;
  for (const Feature *feature : features) {
    // Object parameters
    for (auto &param : feature->shaderParams) {
      objectLayoutBuilder.push(param.name, param.type);
    }

    // Apply modifiers
    if (feature->pipelineModifier) {
      feature->pipelineModifier->buildPipeline(*this);
    }
  }

  // Set shader generator input mesh format
  generator.meshFormat = meshFormat;

  setupShaderOutputFields();

  // Populate shaderEntryPoints from features & builtin entry points
  collectShaderEntryPoints();

  // Collect texture parameters from features/materials
  collectTextureBindings();

  // Setup temporary shader definitions for indexing
  setupShaderDefinitions(deviceLimits, false);

  // Scan shader for accessed buffer/texture bindings
  shader::IndexedBindings indexedShaderBindings = generator.indexBindings(shaderEntryPoints);
  indexedShaderBindings.dump();

  // Remove unused fields from buffer layouts
  optimizeBufferLayouts(indexedShaderBindings);

  // Update shader definitions with optimized layouts
  setupShaderDefinitions(deviceLimits, true);

  buildPipelineLayout(device, deviceLimits);

  buildBaseDrawParameters(output.baseDrawParameters, features);

  // Actually create the pipeline object
  finalize(device);
}

size_t PipelineBuilder::getViewBindGroupIndex() { return 1; }
size_t PipelineBuilder::getDrawBindGroupIndex() { return 0; }

void PipelineBuilder::buildPipelineLayout(WGPUDevice device, const WGPULimits &deviceLimits) {
  assert(!output.pipelineLayout);
  assert(output.bindGroupLayouts.empty());

  std::vector<WGPUBindGroupLayoutEntry> bindGroupLayoutEntries;

  // Create per-draw bind group (0)
  {
    describeBindGroup(drawBindings, getDrawBindGroupIndex(), bindGroupLayoutEntries, deviceLimits);

    // Append texture bindings to per-draw bind group
    output.textureBindingLayout = generator.textureBindingLayout;
    for (auto &desc : generator.textureBindingLayout.bindings) {
      // Interleaved Texture&Sampler bindings (1 each per texture slot)
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

    WGPUBindGroupLayoutDescriptor desc{
        .label = "per-draw",
        .entryCount = uint32_t(bindGroupLayoutEntries.size()),
        .entries = bindGroupLayoutEntries.data(),
    };
    output.bindGroupLayouts.emplace_back(wgpuDeviceCreateBindGroupLayout(device, &desc));
  }

  // Create per-view bind group (1)
  {
    bindGroupLayoutEntries.clear();
    describeBindGroup(viewBindings, getViewBindGroupIndex(), bindGroupLayoutEntries, deviceLimits);

    WGPUBindGroupLayoutDescriptor desc = WGPUBindGroupLayoutDescriptor{
        .label = "per-view",
        .entryCount = uint32_t(bindGroupLayoutEntries.size()),
        .entries = bindGroupLayoutEntries.data(),
    };
    output.bindGroupLayouts.emplace_back(wgpuDeviceCreateBindGroupLayout(device, &desc));
  }

  WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
  pipelineLayoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout *>(output.bindGroupLayouts.data());
  pipelineLayoutDesc.bindGroupLayoutCount = output.bindGroupLayouts.size();
  output.pipelineLayout.reset(wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc));

  // Store run-time binding info
  output.drawBufferBindings.reserve(drawBindings.size());
  output.viewBuffersBindings.reserve(viewBindings.size());
  for (auto &binding : generator.bufferBindings) {
    std::vector<gfx::detail::BufferBinding> *outArray{};
    if (binding.bindGroup == getDrawBindGroupIndex()) {
      outArray = &output.drawBufferBindings;
    } else {
      outArray = &output.viewBuffersBindings;
    }

    auto &outBinding = (*outArray).emplace_back();
    outBinding.layout = binding.layout;
    outBinding.index = binding.binding;
  }
}

// This sets up the buffer/texture definitions for the shader generator
// NOTE: This logic is tied to buildPipelineLayout which  generates the bind group layouts based on the same rules as this
// function
void PipelineBuilder::setupShaderDefinitions(const WGPULimits &deviceLimits, bool isFinal) {
  drawBindings.clear();
  viewBindings.clear();
  generator.bufferBindings.clear();

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

  // Setup per-draw bind group (0)
  {
    size_t bindGroup = getDrawBindGroupIndex();
    size_t bindingCounter = 0;

    describeShaderBindings(drawBindings, isFinal, generator.bufferBindings, bindingCounter, bindGroup, deviceLimits,
                           BindingFrequency::Draw);

    // Append texture to draw bind group
    generator.textureBindingLayout = !isFinal ? textureBindings.getCurrentFinalLayout(bindingCounter, &bindingCounter)
                                              : textureBindings.finalize(bindingCounter, &bindingCounter);
    generator.textureBindGroup = bindGroup;
  }

  // Setup per-view bind group (1)
  {
    size_t bindGroup = getViewBindGroupIndex();
    size_t bindingCounter = 0;

    describeShaderBindings(viewBindings, isFinal, generator.bufferBindings, bindingCounter, bindGroup, deviceLimits,
                           BindingFrequency::View);
  }
}

void PipelineBuilder::setupShaderOutputFields() {
  FieldType colorFieldType(ShaderFieldBaseType::Float32, 4);

  size_t index = 0;
  size_t depthIndex = renderTargetLayout.depthTargetIndex.value_or(~0);
  for (auto &target : renderTargetLayout.targets) {
    // Ignore depth target, it's implicitly bound to z depth
    if (index != depthIndex) {
      auto &formatDesc = getTextureFormatDescription(target.format);
      FieldType fieldType(ShaderFieldBaseType::Float32, formatDesc.numComponents);
      generator.outputFields.emplace_back(target.name, fieldType);
    }
    index++;
  }
}

shader::GeneratorOutput PipelineBuilder::generateShader() { return generator.build(shaderEntryPoints); }

void PipelineBuilder::finalize(WGPUDevice device) {
  FeaturePipelineState pipelineState = computePipelineState(features);

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
  SPDLOG_LOGGER_DEBUG(shader::getLogger(), "Generated WGSL:\n {}", generatorOutput.wgslSource);

  output.renderTargetLayout = renderTargetLayout;

  output.shaderModule.reset(wgpuDeviceCreateShaderModule(device, &moduleDesc));
  assert(output.shaderModule);

  WGPURenderPipelineDescriptor desc = {};
  desc.layout = output.pipelineLayout;

  VertexStateBuilder vertStateBuilder;
  vertStateBuilder.build(desc.vertex, meshFormat, output.shaderModule);

  WGPUFragmentState fragmentState = {};
  fragmentState.entryPoint = "fragment_main";
  fragmentState.module = output.shaderModule;

  // Shared blend state for all color targets
  std::optional<WGPUBlendState> blendState{};
  if (pipelineState.blend.has_value()) {
    blendState = pipelineState.blend.value();
  }

  // Color targets
  std::vector<WGPUColorTargetState> colorTargets;
  WGPUDepthStencilState depthStencilState{};

  size_t depthIndex = output.renderTargetLayout.depthTargetIndex.value_or(~0);
  for (size_t index = 0; index < output.renderTargetLayout.targets.size(); index++) {
    auto &target = output.renderTargetLayout.targets[index];
    if (index == depthIndex) {
      // Depth target
      depthStencilState.format = target.format;
      depthStencilState.depthWriteEnabled = pipelineState.depthWrite.value_or(true);
      depthStencilState.depthCompare = pipelineState.depthCompare.value_or(WGPUCompareFunction_Less);
      depthStencilState.stencilBack.compare = WGPUCompareFunction_Always;
      depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
      desc.depthStencil = &depthStencilState;
    } else {
      // Color target
      WGPUColorTargetState &colorTarget = colorTargets.emplace_back();
      colorTarget.format = target.format;
      colorTarget.writeMask = pipelineState.colorWrite.value_or(WGPUColorWriteMask_All);
      colorTarget.blend = blendState.has_value() ? &blendState.value() : nullptr;
    }
  }

  fragmentState.targets = colorTargets.data();
  fragmentState.targetCount = colorTargets.size();

  desc.fragment = &fragmentState;

  desc.multisample.count = 1;
  desc.multisample.mask = ~0;

  desc.primitive.frontFace = meshFormat.windingOrder == WindingOrder::CCW ? WGPUFrontFace_CCW : WGPUFrontFace_CW;
  if (pipelineState.culling.value_or(true)) {
    desc.primitive.cullMode = pipelineState.flipFrontFace.value_or(false) ? WGPUCullMode_Front : WGPUCullMode_Back;
  } else {
    desc.primitive.cullMode = WGPUCullMode_None;
  }

  switch (meshFormat.primitiveType) {
  case PrimitiveType::TriangleList:
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    break;
  case PrimitiveType::TriangleStrip:
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
    desc.primitive.stripIndexFormat = getWGPUIndexFormat(meshFormat.indexFormat);
    break;
  }

  output.pipeline.reset(wgpuDeviceCreateRenderPipeline(device, &desc));
  if (!output.pipeline)
    throw std::runtime_error("Failed to build pipeline");
}

void PipelineBuilder::collectTextureBindings() {
  for (auto &feature : features) {
    for (auto &textureParam : feature->textureParams) {
      textureBindings.addOrUpdateSlot(textureParam.name, 0);
    }
  }

  // Update default texture coordinates based on material
  for (auto &pair : materialTextureBindings) {
    textureBindings.tryUpdateSlot(pair.first, pair.second.defaultTexcoordBinding);
  }
}

} // namespace gfx

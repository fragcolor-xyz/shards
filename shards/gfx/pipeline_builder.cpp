#include "pipeline_builder.hpp"
#include "enums.hpp"
#include "renderer_types.hpp"
#include "shader/struct_layout.hpp"
#include "log.hpp"
#include "shader/textures.hpp"
#include "shader/generator.hpp"
#include "shader/blocks.hpp"
#include "shader/wgsl_mapping.hpp"
#include "shader/log.hpp"
#include "spdlog/spdlog.h"
#include "gfx_wgpu_pipeline_helper.hpp"
#include <memory>
#include <variant>

using namespace gfx::detail;
using namespace gfx::shader;
namespace gfx {

static auto logger = getLogger();

static void describeShaderBindings(const std::vector<BufferBindingBuilder *> &bindings,
                                   std::vector<shader::BufferBinding> &outShaderBindings, size_t &bindingCounter,
                                   size_t bindGroup, const WGPULimits &deviceLimits) {
  for (auto &builder : bindings) {
    builder->bindGroup = bindGroup;
    builder->binding = bindingCounter++;

    outShaderBindings.emplace_back(shader::BufferBinding{
        .bindGroup = builder->bindGroup,
        .binding = builder->binding,
        .name = builder->name,
        // Use the optimized type when available, original type as a fallback
        .layout = builder->optimizedStructType.value_or(builder->structType),
        .addressSpace = builder->addressSpace,
        .dimension = builder->dimension,
    });
  }
}

static void describeBindGroup(const std::vector<BufferBindingBuilder *> &builders, size_t bindGroupIndex,
                              std::vector<WGPUBindGroupLayoutEntry> &outEntries,
                              std::vector<BufferBindingRef> &outDynamicBufferRefs, const WGPULimits &limits) {
  size_t index = 0;
  for (auto &builder : builders) {
    if (builder->unused)
      continue;

    if (builder->bindGroup != bindGroupIndex)
      continue;

    auto &bglEntry = outEntries.emplace_back();
    bglEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
    bglEntry.binding = builder->binding;

    auto &bufferBinding = bglEntry.buffer;
    bufferBinding.hasDynamicOffset = builder->hasDynamicOffset;

    switch (builder->addressSpace) {
    case shader::AddressSpace::Uniform:
      bufferBinding.type = WGPUBufferBindingType_Uniform;
      break;
    case shader::AddressSpace::Storage:
      bufferBinding.type = WGPUBufferBindingType_ReadOnlyStorage;
      break;
    case shader::AddressSpace::StorageRW:
      bufferBinding.type = WGPUBufferBindingType_Storage;
      break;
    }

    if (builder->hasDynamicOffset)
      outDynamicBufferRefs.push_back(BufferBindingRef{.bindGroup = bindGroupIndex, .bufferIndex = index});

    bufferBinding.minBindingSize = builder->optimizedStructLayout->size;

    ++index;
  }
}

static FeaturePipelineState computePipelineState(const std::vector<const Feature *> &features) {
  FeaturePipelineState state{};
  for (const Feature *feature : features) {
    state = state.combine(feature->state);
  }
  return state;
}

static void buildBaseParameters(ParameterStorage &viewParams, ParameterStorage &drawParams,
                                const std::vector<const Feature *> &features) {
  auto getOutput = [&](auto &param) -> ParameterStorage & {
    return (param.bindGroupId == BindGroupId::Draw) ? drawParams : viewParams;
  };
  for (const Feature *feature : features) {
    for (auto &it : feature->shaderParams) {
      auto &name = it.first;
      auto &param = it.second;
      if (param.defaultValue.index() == 0) {
        // Not set (monostate)
      } else {
        getOutput(param).setParam(name, param.defaultValue);
      }
    }
    for (auto &it : feature->textureParams) {
      auto &name = it.first;
      auto &param = it.second;
      if (param.defaultValue) {
        getOutput(param).setTexture(name, param.defaultValue);
      }
    }
  }
}

BufferBindingBuilder &PipelineBuilder::getOrCreateBufferBinding(FastString name) {
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

void PipelineBuilder::buildOptimalBufferLayouts(const WGPULimits &deviceLimits,
                                                const shader::IndexedBindings &indexedShaderDindings) {
  auto &usedBufferBindings = indexedShaderDindings.bufferBindings;
  for (auto &bufferBinding : bufferBindings) {
    auto it = std::find_if(usedBufferBindings.begin(), usedBufferBindings.end(),
                           [&](auto &binding) { return binding.name == bufferBinding.name; });
    if (it == usedBufferBindings.end()) {
      bufferBinding.unused = true;
      continue;
    }

    StructType &optimalStruct = bufferBinding.optimizedStructType.emplace();
    auto &accessedFields = it->accessedFields;
    StructLayoutBuilder builder(bufferBinding.addressSpace);
    for (auto &f : bufferBinding.structType->entries) {
      auto it = accessedFields.find(f.name);
      if (it != accessedFields.end()) {
        builder.push(f.name, f.type);
        optimalStruct->addField(f.name, f.type);
      }
    }

    // Update alignment requirements
    {
      size_t maxAlign = builder.getCurrentFinalLayout().maxAlignment;

      // Assuming no binding offset is applied, uniform buffers need to be aligned to 16
      // https://www.w3.org/TR/WGSL/#address-space-layout-constraints
      if (bufferBinding.addressSpace == AddressSpace::Uniform)
        maxAlign = alignTo(maxAlign, 16);

      if (bufferBinding.hasDynamicOffset) {
        if (bufferBinding.addressSpace == AddressSpace::Storage)
          maxAlign = alignTo(maxAlign, deviceLimits.minStorageBufferOffsetAlignment);
        else
          maxAlign = alignTo(maxAlign, deviceLimits.minUniformBufferOffsetAlignment);
      }

      auto alignItemOpt = builder.forceAlignmentTo(maxAlign);
      if (alignItemOpt) {
        const StructLayoutItem &item = alignItemOpt->get();
        optimalStruct->addField("_struct_padding_", item.type);
      }
    }

    bufferBinding.optimizedStructLayout = builder.finalize();
  }
}

void PipelineBuilder::build(WGPUDevice device, const WGPULimits &deviceLimits) {
  auto &viewBinding = getOrCreateBufferBinding("view");
  viewBinding.bindGroupId = BindGroupId::View;

  auto &objectBinding = getOrCreateBufferBinding("object");
  objectBinding.bindGroupId = BindGroupId::Draw;

  auto &objectStruct = objectBinding.structType;
  auto &viewStruct = viewBinding.structType;
  auto getStructType = [&](auto &param) -> decltype(objectStruct) & {
    return (param.bindGroupId == BindGroupId::Draw) ? objectStruct : viewStruct;
  };

  for (const Feature *feature : features) {
    // Push parameter declarations
    for (auto &it : feature->shaderParams) {
      getStructType(it.second)->addField(it.first, it.second.type);
    }

    // Apply modifiers
    if (feature->pipelineModifier) {
      feature->pipelineModifier->buildPipeline(*this, options);
    }

    std::vector<std::weak_ptr<Feature>> otherFeatures;
    for (auto &otherFeature : features) {
      if (otherFeature != feature) {
        otherFeatures.push_back(const_cast<Feature *>(otherFeature)->weak_from_this());
      }
    }

    // Store parameter generators
    for (const auto &gen : feature->generators) {
      std::visit(
          [&](auto arg) {
            using T = std::decay_t<decltype(arg)>;

            auto cached = CachedFeatureGenerator<T>{
                .callback = arg,
                .owningFeature = const_cast<Feature *>(feature)->weak_from_this(),
                .otherFeatures = otherFeatures,
            };

            if constexpr (std::is_same_v<T, FeatureGenerator::PerObject>) {
              output.perObjectGenerators.push_back(cached);
            } else if constexpr (std::is_same_v<T, FeatureGenerator::PerView>) {
              output.perViewGenerators.push_back(cached);
            }
          },
          gen.callback);
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
  setupShaderDefinitions(deviceLimits);

  // Scan shader for accessed buffer/texture bindings
  shader::IndexedBindings indexedShaderBindings = generator.indexBindings(shaderEntryPoints);
  indexedShaderBindings.dump();

  // Remove unused fields from buffer layouts
  buildOptimalBufferLayouts(deviceLimits, indexedShaderBindings);

  // Update shader definitions with optimized layouts
  setupShaderDefinitions(deviceLimits);

  buildPipelineLayout(device, deviceLimits);

  buildBaseParameters(output.baseViewParameters, output.baseDrawParameters, features);

  // Actually create the pipeline object
  finalize(device);
}

size_t PipelineBuilder::getViewBindGroupIndex() { return 1; }
size_t PipelineBuilder::getDrawBindGroupIndex() { return 0; }

void PipelineBuilder::buildPipelineLayout(WGPUDevice device, const WGPULimits &deviceLimits) {
  shassert(!output.pipelineLayout);
  shassert(output.bindGroupLayouts.empty());

  std::vector<WGPUBindGroupLayoutEntry> bindGroupLayoutEntries;

  // Create per-draw bind group (0)
  {
    describeBindGroup(drawBindings, getDrawBindGroupIndex(), bindGroupLayoutEntries, output.dynamicBufferRefs, deviceLimits);

    // Append texture bindings to per-draw bind group
    output.textureBindingLayout = generator.textureBindingLayout;
    for (auto &desc : generator.textureBindingLayout.bindings) {
      // Interleaved Texture&Sampler bindings (1 each per texture slot)
      WGPUBindGroupLayoutEntry &textureBinding = bindGroupLayoutEntries.emplace_back();
      textureBinding.binding = desc.binding;
      textureBinding.visibility = WGPUShaderStage_Fragment;
      textureBinding.texture.multisampled = false;
      textureBinding.texture.sampleType = getWGPUSampleType(desc.type.format);
      switch (desc.type.dimension) {
      case TextureDimension::D1:
        textureBinding.texture.viewDimension = WGPUTextureViewDimension_1D;
        break;
      case TextureDimension::D2:
        textureBinding.texture.viewDimension = WGPUTextureViewDimension_2D;
        break;
      case TextureDimension::Cube:
        textureBinding.texture.viewDimension = WGPUTextureViewDimension_Cube;
        break;
      }

      WGPUBindGroupLayoutEntry &samplerBinding = bindGroupLayoutEntries.emplace_back();
      samplerBinding.binding = desc.defaultSamplerBinding;
      samplerBinding.visibility = WGPUShaderStage_Fragment;
      samplerBinding.sampler.type =
          desc.type.format == TextureSampleType::Float ? WGPUSamplerBindingType_Filtering : WGPUSamplerBindingType_NonFiltering;
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
    describeBindGroup(viewBindings, getViewBindGroupIndex(), bindGroupLayoutEntries, output.dynamicBufferRefs, deviceLimits);

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

  // for (auto &binding : generator.bufferBindings) {
  for (auto &binding : bufferBindings) {
    if (binding.unused)
      continue;

    std::vector<gfx::detail::BufferBinding> *outArray{};
    if (binding.bindGroup == getDrawBindGroupIndex()) {
      outArray = &output.drawBufferBindings;
    } else {
      outArray = &output.viewBuffersBindings;
    }

    auto &outBinding = (*outArray).emplace_back();
    outBinding.layout = binding.optimizedStructLayout.value();
    outBinding.index = binding.binding;
    outBinding.dimension = binding.dimension;
    outBinding.name = binding.name;
  }
}

// This sets up the buffer/texture definitions for the shader generator
// NOTE: This logic is tied to buildPipelineLayout which  generates the bind group layouts based on the same rules as this
// function
void PipelineBuilder::setupShaderDefinitions(const WGPULimits &deviceLimits) {
  drawBindings.clear();
  viewBindings.clear();
  generator.bufferBindings.clear();

  // Sort bindings into per draw/view
  for (auto &binding : bufferBindings) {
    switch (binding.bindGroupId) {
    case BindGroupId::Draw:
      drawBindings.push_back(&binding);
      break;
    case BindGroupId::View:
      viewBindings.push_back(&binding);
      break;
    }
  }

  // Setup per-draw bind group (0)
  {
    size_t bindGroup = getDrawBindGroupIndex();
    size_t bindingCounter = 0;

    describeShaderBindings(drawBindings, generator.bufferBindings, bindingCounter, bindGroup, deviceLimits);

    // Append texture to draw bind group
    generator.textureBindingLayout = textureBindings.getCurrentFinalLayout(bindingCounter, &bindingCounter);
    generator.textureBindGroup = bindGroup;
  }

  // Setup per-view bind group (1)
  {
    size_t bindGroup = getViewBindGroupIndex();
    size_t bindingCounter = 0;

    describeShaderBindings(viewBindings, generator.bufferBindings, bindingCounter, bindGroup, deviceLimits);
  }
}

void PipelineBuilder::setupShaderOutputFields() {
  NumType colorFieldType(ShaderFieldBaseType::Float32, 4);

  size_t index = 0;
  size_t depthIndex = renderTargetLayout.depthTargetIndex.value_or(~0);
  for (auto &target : renderTargetLayout.targets) {
    // Ignore depth target, it's implicitly bound to z depth
    if (index != depthIndex) {
      auto &formatDesc = getTextureFormatDescription(target.format);
      NumType fieldType(ShaderFieldBaseType::Float32, formatDesc.numComponents);
      switch (formatDesc.storageType) {
      case StorageType::UInt8:
        fieldType.baseType = ShaderFieldBaseType::UInt8;
        break;
      case StorageType::Int8:
        fieldType.baseType = ShaderFieldBaseType::Int8;
        break;
      case StorageType::UInt16:
        fieldType.baseType = ShaderFieldBaseType::UInt16;
        break;
      case StorageType::Int16:
        fieldType.baseType = ShaderFieldBaseType::Int16;
        break;
      case StorageType::UInt32:
        fieldType.baseType = ShaderFieldBaseType::UInt32;
        break;
      case StorageType::Int32:
        fieldType.baseType = ShaderFieldBaseType::Int32;
        break;
      case StorageType::UNorm8:
      case StorageType::SNorm8:
      case StorageType::UNorm16:
      case StorageType::SNorm16:
      case StorageType::Float16:
      case StorageType::Float32:
        fieldType.baseType = ShaderFieldBaseType::Float32;
        break;
      default:
        throw std::logic_error("Invalid storage type");
      }
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
    shader::GeneratorOutput::dumpErrors(shader::getLogger(), generatorOutput);
    for (auto &err : generatorOutput.errors)
      output.compilationError.emplace(err.error);
    return;
  }

  try {
    output.shaderModule = compileShaderFromWgsl(device, generatorOutput.wgslSource.c_str());
  } catch (const std::runtime_error &ex) {
    SPDLOG_LOGGER_ERROR(logger, "Failed to compile shader code:");
    SPDLOG_LOGGER_ERROR(logger, "{}\n------------------", generatorOutput.wgslSource);
    SPDLOG_LOGGER_ERROR(logger, "{}", ex.what());
    output.compilationError.emplace(ex.what());
    return;
  }

  auto shaderLogger = shader::getLogger();
  if (shaderLogger->should_log(spdlog::level::trace)) {
    shaderLogger->trace("Shader WGSL:\n{}", generatorOutput.wgslSource);
  }

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

  output.renderTargetLayout = renderTargetLayout;

  size_t depthIndex = renderTargetLayout.depthTargetIndex.value_or(~0);
  for (size_t index = 0; index < renderTargetLayout.targets.size(); index++) {
    auto &target = renderTargetLayout.targets[index];
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

  // Flip culling if rendering is inverted
  if (desc.primitive.cullMode != WGPUCullMode_None && this->isRenderingFlipped) {
    desc.primitive.cullMode = desc.primitive.cullMode == WGPUCullMode_Front ? WGPUCullMode_Back : WGPUCullMode_Front;
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
  if (!output.pipeline) {
    output.compilationError.emplace("Failed to build pipeline");
    SPDLOG_LOGGER_DEBUG(logger, "Shader WGSL: \n{}", generatorOutput.wgslSource);
  }
}

void PipelineBuilder::collectTextureBindings() {
  for (auto &feature : features) {
    for (auto &it : feature->textureParams) {
      textureBindings.addOrUpdateSlot(it.first, it.second.type, 0);
    }
  }

  // Update default texture coordinates based on material
  for (auto &pair : materialTextureBindings) {
    textureBindings.tryUpdateSlot(pair.first, pair.second.defaultTexcoordBinding);
  }
}

} // namespace gfx

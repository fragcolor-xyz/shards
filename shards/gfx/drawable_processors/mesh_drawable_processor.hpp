#ifndef FE9A8471_61AF_483B_8D2F_84E570864FB2
#define FE9A8471_61AF_483B_8D2F_84E570864FB2

#include "../drawables/mesh_drawable.hpp"
#include "../drawable_processor.hpp"
#include "../renderer_storage.hpp"
#include "../shader/types.hpp"
#include "../texture_placeholder.hpp"
#include "../sampler_cache.hpp"
#include "../pmr/list.hpp"
#include "../pmr/unordered_map.hpp"
#include "drawable_processor_helpers.hpp"
#include <tracy/Tracy.hpp>

namespace gfx::detail {

struct MeshDrawableProcessor final : public IDrawableProcessor {
  struct DrawableData {
    using allocator_type = shards::pmr::PolymorphicAllocator<>;

    Hash128 groupHash;
    const IDrawable *drawable{};
    size_t queueIndex{}; // Original index in the queue
    MeshContextData *mesh{};
    std::optional<int4> clipRect{};
    shards::pmr::vector<TextureData> textures;
    ParameterStorage parameters; // TODO: Load values directly into buffer
    float projectedDepth{};      // Projected view depth, only calculated when sorting by depth

    DrawableData() = default;
    DrawableData(allocator_type allocator) : textures(allocator), parameters(allocator) {}
    DrawableData(DrawableData &&other, allocator_type allocator) : textures(allocator), parameters(allocator) {
      (*this) = std::move(other);
    }
    DrawableData &operator=(DrawableData &&other) = default;
  };

  struct GroupData {
    WGPUBindGroup drawBindGroup{};
    Hash128 bindGroupHash;
    WgpuHandle<WGPUBindGroup> ownedDrawBindGroup;
  };

  struct PreparedBuffer {
    WGPUBuffer buffer;
    size_t length;
    size_t stride;
  };

  struct PreparedGroupData {
    using allocator_type = shards::pmr::PolymorphicAllocator<>;

    shards::pmr::vector<DrawGroup> groups;    // the group ranges
    shards::pmr::vector<GroupData> groupData; // Data associated with groups

    PreparedGroupData(allocator_type allocator) : groups(allocator), groupData(allocator) {}
  };

  struct PrepareData {
    using allocator_type = shards::pmr::PolymorphicAllocator<>;

    shards::pmr::vector<std::shared_ptr<ContextData>> referencedContextData; // Context data to keep alive

    std::optional<PreparedGroupData> groups;
    shards::pmr::vector<DrawableData *> drawableData{};

    shards::pmr::vector<PreparedBuffer> drawBuffers;
    shards::pmr::vector<PreparedBuffer> viewBuffers;

    WgpuHandle<WGPUBindGroup> viewBindGroup;

    PrepareData(allocator_type allocator) : referencedContextData(allocator), drawBuffers(allocator), viewBuffers(allocator) {}
  };

  struct SharedBufferPool {
  private:
    WGPUBufferPool bufferPool;
    std::mutex mutex;

  public:
    SharedBufferPool(std::function<WGPUBuffer(size_t)> &&init) : bufferPool(std::move(init)) {}

    WGPUBuffer allocate(size_t size) {
      std::lock_guard<std::mutex> _lock(mutex);
      return bufferPool.allocateBuffer(size);
    }

    void reset() {
      std::lock_guard<std::mutex> _lock(mutex);
      bufferPool.reset();
    }
  };

  SharedBufferPool drawBufferPool;
  SharedBufferPool viewBufferPool;
  WGPUSupportedLimits limits{};

  TextureViewCache textureViewCache;
  SamplerCache samplerCache;
  size_t frameCounter{};

  TexturePtr placeholderTextures[3]{
      []() { return PlaceholderTexture::create(TextureDimension::D1, int2(2, 1), float4(1, 1, 1, 1)); }(),
      []() { return PlaceholderTexture::create(TextureDimension::D2, int2(2, 2), float4(1, 1, 1, 1)); }(),
      []() { return PlaceholderTexture::create(TextureDimension::Cube, int2(2, 2), float4(1, 1, 1, 1)); }(),
  };

  MeshDrawableProcessor(Context &context)
      : drawBufferPool(getDrawBufferInitializer(context)), viewBufferPool(getViewBufferInitializer(context)),
        samplerCache(context.wgpuDevice) {
    wgpuDeviceGetLimits(context.wgpuDevice, &limits);
  }

  static std::function<WGPUBuffer(size_t)> getDrawBufferInitializer(Context &context) {
    return [device = context.wgpuDevice](size_t bufferSize) {
      WGPUBufferDescriptor desc{
          .label = "<object buffer>",
          .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
          .size = bufferSize,
      };
      return wgpuDeviceCreateBuffer(device, &desc);
    };
  }

  static std::function<WGPUBuffer(size_t)> getViewBufferInitializer(Context &context) {
    return [device = context.wgpuDevice](size_t bufferSize) {
      WGPUBufferDescriptor desc{
          .label = "<view buffer>",
          .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
          .size = bufferSize,
      };
      return wgpuDeviceCreateBuffer(device, &desc);
    };
  }

  void reset(size_t frameCounter) override {
    ZoneScoped;
    this->frameCounter = frameCounter;
    drawBufferPool.reset();
    viewBufferPool.reset();

    textureViewCache.clearOldCacheItems(frameCounter, 120 * 60 / 2);
  }

  void buildPipeline(PipelineBuilder &builder, const BuildPipelineOptions &options) override {
    using namespace shader;

    auto &viewBinding = builder.getOrCreateBufferBinding("view");
    auto &viewLayoutBuilder = viewBinding.layoutBuilder;
    viewLayoutBuilder.push("view", FieldTypes::Float4x4);
    viewLayoutBuilder.push("proj", FieldTypes::Float4x4);
    viewLayoutBuilder.push("invView", FieldTypes::Float4x4);
    viewLayoutBuilder.push("invProj", FieldTypes::Float4x4);
    viewLayoutBuilder.push("viewport", FieldTypes::Float4);

    auto &objectBinding = builder.getOrCreateBufferBinding("object");
    objectBinding.bufferType = shader::BufferType::Storage;
    objectBinding.hasDynamicOffset = true;
    auto &objectLayoutBuilder = objectBinding.layoutBuilder;
    objectLayoutBuilder.push("world", FieldTypes::Float4x4);
    objectLayoutBuilder.push("invWorld", FieldTypes::Float4x4);
    objectLayoutBuilder.push("invTransWorld", FieldTypes::Float4x4);

    const MeshDrawable &meshDrawable = static_cast<const MeshDrawable &>(builder.firstDrawable);

    builder.meshFormat = meshDrawable.mesh->getFormat();

    if (!options.ignoreDrawableFeatures) {
    for (auto &feature : meshDrawable.features) {
      builder.features.push_back(feature.get());
    }
  }
  }

  void generateDrawableData(DrawableData &data, Context &context, const CachedPipeline &cachedPipeline, const IDrawable *drawable,
                            const ViewData &viewData, const ParameterStorage *baseDrawData, const ParameterStorage *baseViewData,
                            size_t frameCounter, bool needProjectedDepth = false) {
    ZoneScoped;

    const MeshDrawable &meshDrawable = static_cast<const MeshDrawable &>(*drawable);

    // Optionally compute projected depth based on transform center
    if (needProjectedDepth) {
      float4 projected = mul(viewData.cachedView.viewProjectionTransform, mul(meshDrawable.transform, float4(0, 0, 0, 1)));
      data.projectedDepth = projected.z;
    }

    data.drawable = drawable;

    ParameterStorage &parameters = data.parameters;

    parameters.setParam("world", meshDrawable.transform);

    float4x4 worldInverse = linalg::inverse(meshDrawable.transform);
    parameters.setParam("invWorld", worldInverse);
    parameters.setParam("invTransWorld", linalg::transpose(worldInverse));

    auto &textureBindings = cachedPipeline.textureBindingLayout.bindings;
    data.textures.resize(textureBindings.size());

    auto setTextureParameter = [&](const char *name, const TexturePtr &texture) {
      gfx::detail::setTextureParameter(textureBindings, data.textures, context, samplerCache, frameCounter, name, texture);
    };

    auto applyParameters = [&](const auto &srcParams) {
      gfx::detail::applyParameters(textureBindings, data.textures, context, samplerCache, frameCounter, parameters, srcParams);
    };

    // NOTE : The parameters below are ordered by priority, so later entries overwrite previous entries

    // Grab default parameters
    applyParameters(cachedPipeline.baseDrawParameters);

    // Grab parameters from material
    if (Material *material = meshDrawable.material.get()) {
      applyParameters(material->parameters);
    }

    // Grab parameters from drawable
    applyParameters(meshDrawable.parameters);

    // Grab dynamic parameters from view
    // TODO(guusw): Currently store per-view textures in the object buffer
    //    Need to split the binding logic so that they can be on the view buffer as well
    if (baseViewData) {
      for (auto &pair : baseViewData->textures) {
        setTextureParameter(pair.first.c_str(), pair.second.texture);
      }
    }

    // Grab dynamic parameters from drawable
    if (baseDrawData) {
      applyParameters(*baseDrawData);
    }

    std::shared_ptr<MeshContextData> meshContextData = meshDrawable.mesh->contextData;

    data.mesh = meshContextData.get();
    data.clipRect = meshDrawable.clipRect;

    // Validate texture bindings
    for (size_t i = 0; i < textureBindings.size(); i++) {
      auto textureData = data.textures[i].data;
      if (textureData) {
        auto &binding = textureBindings[i];
        auto expectedFormat = binding.type.format;
        auto expectedSampleTypes = getTextureFormatDescription(textureData->format.pixelFormat).compatibleSampleTypes;
        if ((uint8_t(expectedSampleTypes) & uint8_t(expectedFormat)) == 0) {
          throw std::runtime_error(
              fmt::format("Texture format mismatch on {}, expected a {} sample, but format {} does not support it", binding.name,
                          magic_enum::enum_name(expectedSampleTypes), magic_enum::enum_name(textureData->format.pixelFormat)));
        }
      }
    }

    // Generate grouping hash
    HasherXXH128<HasherDefaultVisitor> hasher;
    hasher(size_t(data.mesh));
    hasher(data.clipRect);
    for (auto &texture : data.textures)
      hasher(size_t(texture.id));
    data.groupHash = hasher.getDigest();
  }

  TransientPtr prepare(DrawablePrepareContext &context) override {
    auto &allocator = context.storage.workerMemory;
    const CachedPipeline &cachedPipeline = context.cachedPipeline;

    // NOTE: Memory is thrown away at end of frame, deconstructed by renderer
    PrepareData *prepareData = allocator->new_object<PrepareData>();
    prepareData->drawableData.resize(context.drawables.size());

    // Init placeholder texture
    for (size_t i = 0; i < std::size(placeholderTextures); i++) {
      placeholderTextures[i]->createContextDataConditional(context.context);
    }

    // Allocate/map buffer helpers
    auto allocateBuffers = [&](SharedBufferPool &pool, auto &outBuffers, const auto &bindings, size_t numElements) {
      outBuffers.resize(bindings.size());
      for (size_t i = 0; i < bindings.size(); i++) {
        auto &binding = bindings[i];
        PreparedBuffer &preparedBuffer = outBuffers[i];
        preparedBuffer.stride = binding.layout.getArrayStride();
        preparedBuffer.length = preparedBuffer.stride * numElements;
        preparedBuffer.buffer = pool.allocate(preparedBuffer.length);
      }
    };

    // Allocate and map buffers (per view)
    allocateBuffers(viewBufferPool, prepareData->viewBuffers, cachedPipeline.viewBuffersBindings, 1);

    // Allocate and map buffers (per draw)
    size_t numDrawables = context.drawables.size();
    allocateBuffers(drawBufferPool, prepareData->drawBuffers, cachedPipeline.drawBufferBindings, numDrawables);

    // Prepare mesh & texture buffers
    for (auto &baseParam : cachedPipeline.baseDrawParameters.textures) {
      if (baseParam.second.texture) {
        baseParam.second.texture->createContextDataConditional(context.context);
      }
    }

    for (auto &drawable : context.drawables) {
      const MeshDrawable &meshDrawable = static_cast<const MeshDrawable &>(*drawable);
      meshDrawable.mesh->createContextDataConditional(context.context);
    }

    auto *drawableDatas = allocator->new_object<shards::pmr::list<DrawableData>>();

    // Does the following:
    // - Build mesh data
    // - Collect parameters & texture bindings
    // - Generate batch keys
    bool needProjectedDepth = context.sortMode == SortMode::BackToFront;
    {
      ZoneScopedN("generateDrawableData");
      for (size_t index = 0; index < context.drawables.size(); ++index) {
        const IDrawable *drawable = context.drawables[index];
        auto &drawableData = drawableDatas->emplace_back();

        ParameterStorage *baseDrawData{};
        if (context.generatorData.drawParameters) {
          baseDrawData = &(*context.generatorData.drawParameters)[index];
        }

        // TODO: Temporary to store per-view textures in per-object bind group
        ParameterStorage *baseDrawData1 = context.generatorData.viewParameters;

        generateDrawableData(drawableData, context.context, cachedPipeline, drawable, context.viewData, baseDrawData,
                             baseDrawData1, context.storage.frameCounter, needProjectedDepth);
      }
    }

    // Sort and group drawables
    {
      ZoneScopedN("sortAndGroup");

      // Collect results from generate task
      size_t insertIndex{};
      for (auto &data : *drawableDatas) {
        prepareData->drawableData[insertIndex++] = &data;
      }
      assert(insertIndex == context.drawables.size());

      if (context.sortMode == SortMode::Batch) {
        auto comparison = [](const DrawableData *left, const DrawableData *right) { return left->groupHash < right->groupHash; };
        std::stable_sort(prepareData->drawableData.begin(), prepareData->drawableData.end(), comparison);
      } else if (context.sortMode == SortMode::BackToFront) {
        auto compareBackToFront = [](const DrawableData *left, const DrawableData *right) {
          return left->projectedDepth > right->projectedDepth;
        };
        std::stable_sort(prepareData->drawableData.begin(), prepareData->drawableData.end(), compareBackToFront);
      }

      prepareData->groups.emplace(allocator);
      PreparedGroupData &preparedGroupData = prepareData->groups.value();
      {
        ZoneScopedN("groupDrawables");
        groupDrawables(prepareData->drawableData.size(), preparedGroupData.groups,
                       [&](size_t index) { return prepareData->drawableData[index]->groupHash; });
        preparedGroupData.groupData.resize(preparedGroupData.groups.size());

        for (size_t i = 0; i < preparedGroupData.groups.size(); i++) {
          auto &group = preparedGroupData.groups[i];
          auto &groupData = preparedGroupData.groupData[i];
          auto &firstDrawableData = *prepareData->drawableData[group.startIndex];

          // Generate texture binding hash
          HasherXXH128<HasherDefaultVisitor> hasher;
          for (auto &texture : firstDrawableData.textures)
            hasher(texture ? size_t(texture.id) : size_t(0));
          hasher(group.numInstances);
          groupData.bindGroupHash = hasher.getDigest();
        }
      }
    }

    // Setup view buffer data
    {
      ZoneScopedN("fillViewBuffer");

      // Set hard-coded view parameters (view/projection matrix)
      ParameterStorage viewParameters(allocator);

      // Set default parameters
      for (auto &param : cachedPipeline.baseViewParameters.basic)
        viewParameters.setParam(param.first.c_str(), param.second);

      // Set builtin view paramters (transforms)
      setViewParameters(viewParameters, context.viewData);

      // Merge generator parameters
      auto baseViewData = context.generatorData.viewParameters;
      if (baseViewData) {
        for (auto &pair : baseViewData->basic) {
          viewParameters.setParam(pair.first.c_str(), pair.second);
        }
        // TODO: Bind per-view textures here too once bindings are split
      }

      for (size_t bindingIndex = 0; bindingIndex < cachedPipeline.viewBuffersBindings.size(); bindingIndex++) {
        auto &buffer = prepareData->viewBuffers[bindingIndex];
        auto &binding = cachedPipeline.viewBuffersBindings[bindingIndex];
        uint8_t *stagingBuffer = (uint8_t *)allocator->allocate(buffer.length);
        packDrawData(stagingBuffer, buffer.stride, binding.layout, viewParameters);
        wgpuQueueWriteBuffer(context.context.wgpuQueue, buffer.buffer, 0, stagingBuffer, buffer.length);
      }
    }

    // Setup draw buffer data
    {
      ZoneScopedN("fillDrawBuffer");

      PreparedGroupData &preparedGroupData = prepareData->groups.value();

      for (size_t bindingIndex = 0; bindingIndex < cachedPipeline.drawBufferBindings.size(); bindingIndex++) {
        auto &buffer = prepareData->drawBuffers[bindingIndex];
        auto &binding = cachedPipeline.drawBufferBindings[bindingIndex];
        uint8_t *stagingBuffer = (uint8_t *)allocator->allocate(buffer.length);
        for (auto &group : preparedGroupData.groups) {
          size_t offset = buffer.stride * group.startIndex;
          for (size_t i = 0; i < group.numInstances; i++) {
            packDrawData(stagingBuffer + offset, buffer.stride, binding.layout,
                         prepareData->drawableData[group.startIndex + i]->parameters);
            offset += buffer.stride;
          }
        }
        wgpuQueueWriteBuffer(context.context.wgpuQueue, buffer.buffer, 0, stagingBuffer, buffer.length);
      }
    }

    // Generate bind groups
    WGPUBindGroupLayout viewBindGroupLayout = cachedPipeline.bindGroupLayouts[PipelineBuilder::getViewBindGroupIndex()];

    BindGroupBuilder viewBindGroupBuilder(allocator);
    for (size_t i = 0; i < prepareData->viewBuffers.size(); i++) {
      viewBindGroupBuilder.addBinding(cachedPipeline.viewBuffersBindings[i], prepareData->viewBuffers[i].buffer);
    }
    prepareData->viewBindGroup.reset(viewBindGroupBuilder.finalize(context.context.wgpuDevice, viewBindGroupLayout));

    {
      ZoneScopedN("createDrawBindGroups");

      PreparedGroupData &preparedGroupData = prepareData->groups.value();
      WGPUBindGroupLayout drawBindGroupLayout = cachedPipeline.bindGroupLayouts[PipelineBuilder::getDrawBindGroupIndex()];

      auto cachedBindGroups = allocator->new_object<shards::pmr::unordered_map<Hash128, WGPUBindGroup>>();
      for (size_t index = 0; index < preparedGroupData.groups.size(); ++index) {
        shards::pmr::unordered_map<Hash128, WGPUBindGroup> &cache = *cachedBindGroups;

        auto &group = preparedGroupData.groups[index];
        auto &groupData = preparedGroupData.groupData[index];
        auto &firstDrawableData = *prepareData->drawableData[group.startIndex];

        auto it = cache.find(groupData.bindGroupHash);
        if (it == cache.end()) {
          BindGroupBuilder drawBindGroupBuilder(allocator);
          for (size_t i = 0; i < prepareData->drawBuffers.size(); i++) {
            auto &binding = cachedPipeline.drawBufferBindings[i];
            // Drawable buffers are mapped entirely, dynamic offset is applied during encoding
            drawBindGroupBuilder.addBinding(binding, prepareData->drawBuffers[i].buffer, group.numInstances);
          }

          // TODO(guusw): Need to split this so that we can have per-view texture bindings
          for (size_t i = 0; i < cachedPipeline.textureBindingLayout.bindings.size(); i++) {
            auto &binding = cachedPipeline.textureBindingLayout.bindings[i];
            auto texture = firstDrawableData.textures[i];
            if (texture) {
              drawBindGroupBuilder.addTextureBinding(binding, textureViewCache.getDefaultTextureView(frameCounter, *texture.data),
                                                     texture.sampler);
            } else {
              auto &placeholder = placeholderTextures[size_t(binding.type.dimension)];
              WGPUSampler sampler = samplerCache.getDefaultSampler(frameCounter, true, placeholder);
              drawBindGroupBuilder.addTextureBinding(
                  binding, textureViewCache.getDefaultTextureView(frameCounter, *placeholder->contextData.get()), sampler);
            }
          }

          groupData.ownedDrawBindGroup.reset(drawBindGroupBuilder.finalize(context.context.wgpuDevice, drawBindGroupLayout));
          groupData.drawBindGroup = groupData.ownedDrawBindGroup;
          cache.emplace(groupData.bindGroupHash, groupData.drawBindGroup);
        } else {
          groupData.drawBindGroup = it->second;
        }
      }
    }

    return prepareData;
  }

  void encode(DrawableEncodeContext &context) override {
    const PrepareData &prepareData = context.preparedData.get<PrepareData>();
    const ViewData &viewData = context.viewData;
    const CachedPipeline &cachedPipeline = context.cachedPipeline;
    auto encoder = context.encoder;

    wgpuRenderPassEncoderSetPipeline(encoder, cachedPipeline.pipeline);
    wgpuRenderPassEncoderSetBindGroup(encoder, PipelineBuilder::getViewBindGroupIndex(), prepareData.viewBindGroup, 0, nullptr);

    auto &groups = prepareData.groups.value();
    for (size_t i = 0; i < groups.groups.size(); i++) {
      auto &group = groups.groups[i];
      auto &groupData = groups.groupData[i];
      auto &firstDrawable = *prepareData.drawableData[group.startIndex];

      if (firstDrawable.clipRect) {
        auto clipRect = firstDrawable.clipRect.value();

        int2 viewportMin = int2(viewData.viewport.x, viewData.viewport.y);
        int2 viewportMax = int2(viewData.viewport.getX1(), viewData.viewport.getY1());

        // Clamp to viewport
        clipRect.x = linalg::clamp(clipRect.x, viewportMin.x, viewportMax.x);
        clipRect.y = linalg::clamp(clipRect.y, viewportMin.y, viewportMax.y);
        clipRect.z = linalg::clamp(clipRect.z, viewportMin.x, viewportMax.x);
        clipRect.w = linalg::clamp(clipRect.w, viewportMin.y, viewportMax.y);
        int w = clipRect.z - clipRect.x;
        int h = clipRect.w - clipRect.y;
        if (w == 0 || h == 0)
          continue; // Discard draw call instead, wgpu doesn't allow w/h == 0
        wgpuRenderPassEncoderSetScissorRect(encoder, clipRect.x, clipRect.y, w, h);
      } else {
        wgpuRenderPassEncoderSetScissorRect(encoder, viewData.viewport.x, viewData.viewport.y, viewData.viewport.width,
                                            viewData.viewport.height);
      }

      auto &binding = cachedPipeline.drawBufferBindings[0];
      uint32_t dynamicOffsets[] = {uint32_t(group.startIndex * binding.layout.getArrayStride())};
      assert(dynamicOffsets[0] < binding.layout.getArrayStride() * prepareData.drawableData.size());
      wgpuRenderPassEncoderSetBindGroup(encoder, PipelineBuilder::getDrawBindGroupIndex(), groupData.drawBindGroup,
                                        std::size(dynamicOffsets), dynamicOffsets);

      MeshContextData *meshContextData = firstDrawable.mesh;
      assert(meshContextData->vertexBuffer);
      wgpuRenderPassEncoderSetVertexBuffer(encoder, 0, meshContextData->vertexBuffer, 0, meshContextData->vertexBufferLength);

      if (meshContextData->indexBuffer) {
        WGPUIndexFormat indexFormat = getWGPUIndexFormat(meshContextData->format.indexFormat);
        wgpuRenderPassEncoderSetIndexBuffer(encoder, meshContextData->indexBuffer, indexFormat, 0,
                                            meshContextData->indexBufferLength);

        wgpuRenderPassEncoderDrawIndexed(encoder, (uint32_t)meshContextData->numIndices, group.numInstances, 0, 0, 0);
      } else {
        wgpuRenderPassEncoderDraw(encoder, (uint32_t)meshContextData->numVertices, group.numInstances, 0, 0);
      }
    }
  }
};
} // namespace gfx::detail

#endif /* FE9A8471_61AF_483B_8D2F_84E570864FB2 */

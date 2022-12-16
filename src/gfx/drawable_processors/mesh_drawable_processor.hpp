#ifndef FE9A8471_61AF_483B_8D2F_84E570864FB2
#define FE9A8471_61AF_483B_8D2F_84E570864FB2

#include "drawables/mesh_drawable.hpp"
#include "../drawable_processor.hpp"
#include "../renderer_types.hpp"
#include "../shader/types.hpp"
#include "../texture_placeholder.hpp"
#include "drawable_processor_helpers.hpp"
#include "async.hpp"
#include <scoped_allocator>

namespace gfx::detail {

static std::shared_ptr<PlaceholderTexture> placeholderTexture = []() {
  return std::make_shared<PlaceholderTexture>(int2(2, 2), float4(1, 1, 1, 1));
}();

struct MeshDrawableProcessor final : public IDrawableProcessor {
  struct DrawableData {
    using allocator_type = pmr::polymorphic_allocator<>;

    Hash128 groupHash;
    const IDrawable *drawable{};
    MeshContextData *mesh{};
    std::optional<int4> clipRect{};
    pmr::vector<TextureContextData *> textures;
    ParameterStorage parameters; // TODO: Load values directly into buffer

    DrawableData() = default;
    DrawableData(allocator_type allocator) : textures(allocator), parameters(allocator) {}
    DrawableData(DrawableData &&other, allocator_type allocator) : textures(allocator), parameters(allocator) {
      (*this) = std::move(other);
    }
    DrawableData &operator=(DrawableData &&other) = default;
  };

  struct GroupData {
    WgpuHandle<WGPUBindGroup> drawBindGroup;
  };

  struct PreparedBuffer {
    WGPUBuffer buffer;
    size_t length;
    size_t stride;
    WGPUBufferMapAsyncStatus mappingStatus = WGPUBufferMapAsyncStatus_Unknown;
  };

  struct PrepareData {
    using allocator_type = pmr::polymorphic_allocator<>;

    pmr::vector<std::shared_ptr<ContextData>> referencedContextData; // Context data to keep alive
    pmr::vector<DrawGroup> groups;                                   // the group ranges
    pmr::vector<GroupData> groupData;                                // Data associated with groups

    pmr::vector<DrawableData *> drawableData{};

    pmr::vector<PreparedBuffer> drawBuffers;
    pmr::vector<PreparedBuffer> viewBuffers;

    WgpuHandle<WGPUBindGroup> viewBindGroup;

    PrepareData(std::allocator_arg_t, const allocator_type &allocator)
        : referencedContextData(allocator), groups(allocator), groupData(allocator), drawBuffers(allocator),
          viewBuffers(allocator) {}
    PrepareData(PrepareData &&other, allocator_type allocator)
        : referencedContextData(allocator), groups(allocator), groupData(allocator), drawBuffers(allocator),
          viewBuffers(allocator) {
      (*this) = std::move(other);
    }
    PrepareData &operator=(PrepareData &&other) = default;
    ~PrepareData() { SPDLOG_INFO("Destroying prepare data"); }
  };

  struct WorkerData {
    using construct_with_thread_index = std::true_type;

    WGPUBufferPool drawBufferPool;
    WGPUBufferPool viewBufferPool;

    WorkerData(Context &context) {
      drawBufferPool = WGPUBufferPool([device = context.wgpuDevice](size_t bufferSize) {
        WGPUBufferDescriptor desc{
            .label = "<object buffer>",
            .usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_Storage,
            .size = bufferSize,
        };
        return wgpuDeviceCreateBuffer(device, &desc);
      });
      viewBufferPool = WGPUBufferPool([device = context.wgpuDevice](size_t bufferSize) {
        WGPUBufferDescriptor desc{
            .label = "<view buffer>",
            .usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_Uniform,
            .size = bufferSize,
        };
        return wgpuDeviceCreateBuffer(device, &desc);
      });
    }

    void reset() {
      drawBufferPool.reset();
      viewBufferPool.reset();
    }
  };

  TWorkerThreadData<WorkerData> workerData;
  WGPUSupportedLimits limits{};

  MeshDrawableProcessor(Context &context) : workerData(context.executor, std::ref(context)) {
    wgpuDeviceGetLimits(context.wgpuDevice, &limits);
  }

  void reset(size_t frameCounter) override {
    for (auto &data : workerData)
      data.reset();
  }

  void buildPipeline(PipelineBuilder &builder) override {
    using namespace shader;

    auto &viewBinding = builder.getOrCreateBufferBinding("view");
    auto &viewLayoutBuilder = viewBinding.layoutBuilder;
    viewLayoutBuilder.push("view", FieldTypes::Float4x4);
    viewLayoutBuilder.push("proj", FieldTypes::Float4x4);
    viewLayoutBuilder.push("invView", FieldTypes::Float4x4);
    viewLayoutBuilder.push("invProj", FieldTypes::Float4x4);
    viewLayoutBuilder.push("viewport", FieldTypes::Float4);

    auto &objectBinding = builder.getOrCreateBufferBinding("object");
    auto &objectLayoutBuilder = objectBinding.layoutBuilder;
    objectLayoutBuilder.push("world", FieldTypes::Float4x4);
    objectLayoutBuilder.push("invWorld", FieldTypes::Float4x4);
    objectLayoutBuilder.push("invTransWorld", FieldTypes::Float4x4);

    const MeshDrawable &meshDrawable = static_cast<const MeshDrawable &>(builder.firstDrawable);

    builder.meshFormat = meshDrawable.mesh->getFormat();
    for (auto &feature : meshDrawable.features) {
      builder.features.push_back(feature.get());
    }
  }

  void generateDrawableData(DrawableData &data, Context &context, const CachedPipeline &cachedPipeline,
                            const IDrawable *drawable) {
    const MeshDrawable &meshDrawable = static_cast<const MeshDrawable &>(*drawable);

    data.drawable = drawable;

    ParameterStorage &parameters = data.parameters;

    parameters.setParam("world", meshDrawable.transform);

    float4x4 worldInverse = linalg::inverse(meshDrawable.transform);
    parameters.setParam("invWorld", worldInverse);
    parameters.setParam("invTransWorld", linalg::transpose(worldInverse));

    auto &textureBindings = cachedPipeline.textureBindingLayout.bindings;
    data.textures.resize(textureBindings.size());

    auto mapTextureBinding = [&](const char *name) -> int32_t {
      auto it = std::find_if(textureBindings.begin(), textureBindings.end(), [&](auto it) { return it.name == name; });
      if (it != textureBindings.end())
        return int32_t(it - textureBindings.begin());
      return -1;
    };

    auto setTextureParameter = [&](const char *name, const TexturePtr &texture) {
      int32_t targetSlot = mapTextureBinding(name);
      if (targetSlot >= 0) {
        data.textures[targetSlot] = texture->contextData.get();
      }
    };

    // Grab parameters from material
    if (Material *material = meshDrawable.material.get()) {
      for (auto &pair : material->parameters.basic) {
        parameters.setParam(pair.first, pair.second);
      }
      for (auto &pair : material->parameters.texture) {
        setTextureParameter(pair.first.c_str(), pair.second.texture);
      }
    }

    // Grab parameters from drawable
    for (auto &pair : meshDrawable.parameters.basic) {
      parameters.setParam(pair.first, pair.second);
    }
    for (auto &pair : meshDrawable.parameters.texture) {
      setTextureParameter(pair.first.c_str(), pair.second.texture);
    }

    // Set base parameters where unset
    for (auto &baseParam : cachedPipeline.baseDrawParameters.data) {
      parameters.setParamIfUnset(baseParam.first, baseParam.second);
    }

    for (auto &baseParam : cachedPipeline.baseDrawParameters.textures) {
      int32_t targetSlot = mapTextureBinding(baseParam.first.c_str());
      if (targetSlot >= 0 && !data.textures[targetSlot]) {
        baseParam.second.texture->createContextDataConditional(context);
        data.textures[targetSlot] = baseParam.second.texture->contextData.get();
      }
    }

    std::shared_ptr<MeshContextData> meshContextData = meshDrawable.mesh->contextData;

    data.mesh = meshContextData.get();
    data.clipRect = meshDrawable.clipRect;

    // Generate grouping hash
    HasherXXH128<HasherDefaultVisitor> hasher;
    hasher(size_t(data.mesh));
    hasher(data.clipRect);
    for (auto &texture : data.textures)
      hasher(size_t(texture));
    data.groupHash = hasher.getDigest();
  }

  static std::function<void(tf::Subflow &)> generateBufferMapWaitTask(Context &context, PrepareData *prepareData) {
    using namespace std::placeholders;
    std::function<void(tf::Subflow &)> task = std::bind(&waitForBufferMapRecursive, std::ref(context), prepareData, _1);
    return task;
  }

  static void waitForBufferMapRecursive(Context &context, PrepareData *prepareData, tf::Subflow &subflow) {
    bool everythingMapped = true;
    auto check = [&](auto buffers) {
      for (auto &drawBuffer : buffers) {
        if (drawBuffer.mappingStatus == WGPUBufferMapAsyncStatus_Unknown)
          everythingMapped = false;
        else {
          if (drawBuffer.mappingStatus != WGPUBufferMapAsyncStatus_Success)
            throw std::runtime_error("Failed to map WGPUBuffer");
        }
      }
    };
    check(prepareData->drawBuffers);
    check(prepareData->viewBuffers);

    if (!everythingMapped) {
      wgpuDevicePoll(context.wgpuDevice, false, nullptr);
      subflow.emplace(generateBufferMapWaitTask(context, prepareData));
    }
  }

  ProcessorDynamicValue prepare(DrawablePrepareContext &context) override {
    auto &executor = context.context.executor;
    auto &rootTaskAllocator = context.context.workerMemory.get();
    auto &subflow = context.subflow;
    const CachedPipeline &cachedPipeline = context.cachedPipeline;

    // NOTE: Memory is thrown away at end of frame, deconstructed by renderer
    PrepareData *prepareData = rootTaskAllocator->new_object<PrepareData>();
    prepareData->drawableData.resize(context.drawables.size());

    // Init placeholder texture
    placeholderTexture->createContextDataConditional(context.context);
    auto placeholderTextureContextData = placeholderTexture->contextData;

    // Allocate/map buffer helpers
    auto bufferMapCallback = [](WGPUBufferMapAsyncStatus status, void *userData) {
      ((PreparedBuffer *)userData)->mappingStatus = status;
    };
    auto allocateBuffers = [&](WGPUBufferPool &pool, auto &outBuffers, const auto &bindings, size_t numElements) {
      outBuffers.resize(bindings.size());
      for (size_t i = 0; i < bindings.size(); i++) {
        auto &binding = bindings[i];
        PreparedBuffer &preparedBuffer = outBuffers[i];
        preparedBuffer.stride = alignTo(binding.layout.size, limits.limits.minStorageBufferOffsetAlignment);
        preparedBuffer.length = preparedBuffer.stride * numElements;
        preparedBuffer.buffer = pool.allocateBuffer(preparedBuffer.length);
        wgpuBufferMapAsync(preparedBuffer.buffer, WGPUMapMode_Write, 0, preparedBuffer.length, bufferMapCallback,
                           &preparedBuffer);
      }
    };

    // Allocate and map buffers (per view)
    auto &viewBufferPool = workerData.get().viewBufferPool;
    allocateBuffers(viewBufferPool, prepareData->viewBuffers, cachedPipeline.viewBuffersBindings, 1);

    // Allocate and map buffers (per draw)
    size_t numDrawables = context.drawables.size();
    auto &drawBufferPool = workerData.get().drawBufferPool;
    allocateBuffers(drawBufferPool, prepareData->drawBuffers, cachedPipeline.drawBufferBindings, numDrawables);

    // Prepare mesh & texture buffers
    // TODO: Remove context data
    bool *finished = rootTaskAllocator->new_object<bool>(false);
    auto createContextDataTask = subflow.emplace([=, &drawables = context.drawables, &context = context.context]() {
      for (auto &drawable : drawables) {
        const MeshDrawable &meshDrawable = static_cast<const MeshDrawable &>(*drawable);
        meshDrawable.mesh->createContextDataConditional(context);

        for (auto &texParam : meshDrawable.parameters.texture) {
          auto texture = texParam.second.texture;
          if (texture)
            texture->createContextDataConditional(context);
        }

        auto material = meshDrawable.material;
        if (material) {
          for (auto &texParam : material->parameters.texture) {
            auto texture = texParam.second.texture;
            if (texture)
              texture->createContextDataConditional(context);
          }
        }
      }
      *finished = true;
    });

    // Setup collectors
    auto *collector = rootTaskAllocator->new_object<TWorkerCollector<pmr::list<DrawableData>>>(
        std::ref(executor), std::ref(context.context.workerMemory));

    // Does the following:
    // - Build mesh data
    // - Collect parameters & texture bindings
    // - Generate batch keys
    auto generateDrawableData = [=, &drawables = context.drawables, &cachedPipeline](size_t index) {
      auto &drawableData = collector->get().emplace_back();
      this->generateDrawableData(drawableData, context.context, cachedPipeline, drawables[index]);
    };
    auto generateDrawableDataTask =
        subflow
            .emplace([=](tf::Subflow &subflow) {
              assert(*finished);
              subflow.for_each_index(size_t(0), context.drawables.size(), size_t(1), generateDrawableData);
            })
            .succeed(createContextDataTask);

    // Wait for draw buffer to be mapped so we can fill them in the next task
    auto waitForBufferBindingTask =
        subflow.emplace(generateBufferMapWaitTask(context.context, prepareData)).succeed(generateDrawableDataTask);

    // Sort and group drawables
    auto sortAndGroup = [=, &cachedPipeline]() {
      auto &allocator = context.context.workerMemory.get();

      // Collect results from generate task
      size_t insertIndex{};
      for (auto &list : *collector) {
        for (auto &data : list) {
          prepareData->drawableData[insertIndex++] = &data;
        }
      }
      assert(insertIndex == context.drawables.size());

      if (context.sortMode == SortMode::Batch) {
        auto comparison = [](const DrawableData *left, const DrawableData *right) { return left->groupHash < right->groupHash; };
        std::sort(prepareData->drawableData.begin(), prepareData->drawableData.end(), comparison);
      }

      groupDrawables(prepareData->drawableData.size(), prepareData->groups,
                     [&](size_t index) { return prepareData->drawableData[index]->groupHash; });
      prepareData->groupData.resize(prepareData->groups.size());

      // Setup view buffer data
      {
        ParameterStorage viewParameters(allocator);
        setViewParameters(viewParameters, context.viewData);

        auto &buffer = prepareData->viewBuffers[0];
        auto &binding = cachedPipeline.viewBuffersBindings[0];
        uint8_t *bufferData = (uint8_t *)wgpuBufferGetMappedRange(buffer.buffer, 0, buffer.length);
        packDrawData(bufferData, buffer.stride, binding.layout, viewParameters);
      }

      // Setup draw buffer data
      {
        auto &buffer = prepareData->drawBuffers[0];
        auto &binding = cachedPipeline.drawBufferBindings[0];
        uint8_t *bufferData = (uint8_t *)wgpuBufferGetMappedRange(buffer.buffer, 0, buffer.length);
        for (auto &group : prepareData->groups) {
          size_t offset = buffer.stride * group.startIndex;
          for (size_t i = 0; i < group.numInstances; i++) {
            packDrawData(bufferData + offset, buffer.stride, binding.layout,
                         prepareData->drawableData[group.startIndex + i]->parameters);
            offset += buffer.stride;
          }
        }
      }

      auto unmapBuffers = [&](auto &vec) {
        for (auto &buffer : vec)
          wgpuBufferUnmap(buffer.buffer);
      };
      unmapBuffers(prepareData->drawBuffers);
      unmapBuffers(prepareData->viewBuffers);

      // Generate bind groups
      WGPUBindGroupLayout drawBindGroupLayout = cachedPipeline.bindGroupLayouts[PipelineBuilder::getDrawBindGroupIndex()];
      WGPUBindGroupLayout viewBindGroupLayout = cachedPipeline.bindGroupLayouts[PipelineBuilder::getViewBindGroupIndex()];

      BindGroupBuilder viewBindGroupBuilder(allocator);
      for (size_t i = 0; i < prepareData->viewBuffers.size(); i++) {
        viewBindGroupBuilder.addBinding(cachedPipeline.viewBuffersBindings[i], prepareData->viewBuffers[i].buffer);
      }
      prepareData->viewBindGroup.reset(viewBindGroupBuilder.finalize(context.context.wgpuDevice, viewBindGroupLayout));

      for (size_t i = 0; i < prepareData->groups.size(); i++) {
        auto &group = prepareData->groups[i];
        auto &groupData = prepareData->groupData[i];
        auto &firstDrawableData = *prepareData->drawableData[group.startIndex];

        BindGroupBuilder drawBindGroupBuilder(allocator);
        for (size_t i = 0; i < prepareData->drawBuffers.size(); i++) {
          auto &binding = cachedPipeline.drawBufferBindings[i];
          drawBindGroupBuilder.addBinding(binding, prepareData->drawBuffers[i].buffer, group.numInstances, group.startIndex);
        }
        for (size_t i = 0; i < cachedPipeline.textureBindingLayout.bindings.size(); i++) {
          auto &binding = cachedPipeline.textureBindingLayout.bindings[i];
          auto texture = firstDrawableData.textures[i];
          if (texture) {
            drawBindGroupBuilder.addTextureBinding(binding, texture->defaultView, texture->sampler);
          } else {
            drawBindGroupBuilder.addTextureBinding(binding, placeholderTextureContextData->textureView,
                                                   placeholderTextureContextData->sampler);
          }
        }

        groupData.drawBindGroup.reset(drawBindGroupBuilder.finalize(context.context.wgpuDevice, drawBindGroupLayout));
      }
    };
    auto sortAndGroupTask = subflow.emplace(sortAndGroup).succeed(waitForBufferBindingTask);

    return prepareData;
  }

  void encode(DrawableEncodeContext &context) override {
    return;
    const PrepareData &prepareData = context.preparedData.get<PrepareData>();
    const ViewData &viewData = context.viewData;
    auto encoder = context.encoder;

    wgpuRenderPassEncoderSetPipeline(encoder, context.cachedPipeline.pipeline);
    wgpuRenderPassEncoderSetBindGroup(encoder, PipelineBuilder::getViewBindGroupIndex(), prepareData.viewBindGroup, 0, nullptr);

    for (size_t i = 0; i < prepareData.groups.size(); i++) {
      auto &group = prepareData.groups[i];
      auto &groupData = prepareData.groupData[i];
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

      wgpuRenderPassEncoderSetBindGroup(encoder, PipelineBuilder::getDrawBindGroupIndex(), groupData.drawBindGroup, 0, nullptr);

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

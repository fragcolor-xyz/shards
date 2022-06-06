#include "renderer.hpp"
#include "context.hpp"
#include "drawable.hpp"
#include "feature.hpp"
#include "hasherxxh128.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "params.hpp"
#include "renderer_types.hpp"
#include "shader/blocks.hpp"
#include "shader/entry_point.hpp"
#include "shader/fmt.hpp"
#include "shader/generator.hpp"
#include "shader/textures.hpp"
#include "shader/uniforms.hpp"
#include "texture.hpp"
#include "texture_placeholder.hpp"
#include "view.hpp"
#include "view_texture.hpp"
#include <algorithm>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

#define GFX_RENDERER_MAX_BUFFERED_FRAMES (2)

namespace gfx {
using shader::FieldType;
using shader::FieldTypes;
using shader::TextureBindingLayout;
using shader::TextureBindingLayoutBuilder;
using shader::UniformBufferLayout;
using shader::UniformBufferLayoutBuilder;
using shader::UniformLayout;

PipelineStepPtr makeDrawablePipelineStep(RenderDrawablesStep &&step) { return std::make_shared<PipelineStep>(std::move(step)); }

typedef uint16_t TextureId;

struct TextureIds {
  std::vector<TextureId> textures;

  bool operator==(const TextureIds &other) const {
    return std::equal(textures.begin(), textures.end(), other.textures.begin(), other.textures.end());
  }

  bool operator!=(const TextureIds &other) const { return !(*this == other); }

  bool operator<(const TextureIds &other) const {
    if (textures.size() != other.textures.size())
      return textures.size() < other.textures.size();
    for (size_t i = 0; i < textures.size(); i++) {
      if (textures[i] != other.textures[i]) {
        return textures[i] < other.textures[i];
      }
    }
    return false;
  }
};

struct TextureIdMap {
private:
  std::map<Texture *, TextureId> mapping;
  std::vector<std::shared_ptr<TextureContextData>> textureData;

public:
  TextureId assign(Context &context, Texture *texture) {
    if (!texture)
      return ~0;

    auto it = mapping.find(texture);
    if (it != mapping.end()) {
      return it->second;
    } else {
      texture->createContextDataConditional(context);

      size_t newId = textureData.size();
      assert(newId <= UINT16_MAX);

      TextureId id = TextureId(newId);
      textureData.emplace_back(texture->contextData);
      mapping.insert_or_assign(texture, id);
      return id;
    }
  }
  TextureContextData *resolve(TextureId id) const {
    if (id == TextureId(~0))
      return nullptr;
    else {
      return textureData[id].get();
    }
  }

  void reset() {
    mapping.clear();
    textureData.clear();
  }
};

struct SortableDrawable;
struct DrawGroupKey {
  MeshContextData *meshData{};
  TextureIds textures{};

  DrawGroupKey() = default;
  DrawGroupKey(const Drawable &drawable, const TextureIds &textureIds)
      : meshData(drawable.mesh->contextData.get()), textures(textureIds) {}

  bool operator!=(const DrawGroupKey &other) const { return meshData != other.meshData || textures != other.textures; }
  bool operator<(const DrawGroupKey &other) const {
    if (meshData != other.meshData) {
      return size_t(meshData) < size_t(other.meshData);
    }
    return textures < other.textures;
  }
};

struct SortableDrawable {
  Drawable *drawable{};
  TextureIds textureIds;
  DrawGroupKey key;
  float projectedDepth = 0.0f;
};

struct CachedPipeline {
  struct DrawGroup {
    DrawGroupKey key;
    size_t startIndex;
    size_t numInstances;
    DrawGroup(const DrawGroupKey &key, size_t startIndex, size_t numInstances)
        : key(key), startIndex(startIndex), numInstances(numInstances) {}
  };

  MeshFormat meshFormat;
  std::vector<const Feature *> features;
  UniformBufferLayout objectBufferLayout;
  TextureBindingLayout textureBindingLayout;
  DrawData baseDrawData;

  WGPURenderPipeline pipeline = {};
  WGPUShaderModule shaderModule = {};
  WGPUPipelineLayout pipelineLayout = {};
  std::vector<WGPUBindGroupLayout> bindGroupLayouts;

  std::vector<Drawable *> drawables;
  std::vector<SortableDrawable> drawablesSorted;
  std::vector<DrawGroup> drawGroups;
  TextureIdMap textureIdMap;

  DynamicWGPUBufferPool instanceBufferPool;

  void resetDrawables() {
    drawables.clear();
    drawablesSorted.clear();
    drawGroups.clear();
    textureIdMap.reset();
  }

  void resetPools() { instanceBufferPool.reset(); }

  void release() {
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuShaderModuleRelease(shaderModule);
    wgpuRenderPipelineRelease(pipeline);
    for (WGPUBindGroupLayout &layout : bindGroupLayouts) {
      wgpuBindGroupLayoutRelease(layout);
    }
  }

  ~CachedPipeline() { release(); }
};
typedef std::shared_ptr<CachedPipeline> CachedPipelinePtr;

static void packDrawData(uint8_t *outData, size_t outSize, const UniformBufferLayout &layout, const DrawData &drawData) {
  size_t layoutIndex = 0;
  for (auto &fieldName : layout.fieldNames) {
    auto drawDataIt = drawData.data.find(fieldName);
    if (drawDataIt != drawData.data.end()) {
      const UniformLayout &itemLayout = layout.items[layoutIndex];
      FieldType drawDataFieldType = getParamVariantType(drawDataIt->second);
      if (itemLayout.type != drawDataFieldType) {
        spdlog::warn("WEBGPU: Field type mismatch layout:{} drawData:{}", itemLayout.type, drawDataFieldType);
        continue;
      }

      packParamVariant(outData + itemLayout.offset, outSize - itemLayout.offset, drawDataIt->second);
    }
    layoutIndex++;
  }
}

struct CachedViewData {
  DynamicWGPUBufferPool viewBuffers;
  float4x4 projectionMatrix;
  ~CachedViewData() {}

  void resetPools() { viewBuffers.reset(); }
};
typedef std::shared_ptr<CachedViewData> CachedViewDataPtr;

struct FrameReferences {
  std::vector<std::shared_ptr<ContextData>> contextDataReferences;
  void clear() { contextDataReferences.clear(); }
};

struct RendererImpl final : public ContextData {
  Context &context;
  WGPUSupportedLimits deviceLimits = {};

  UniformBufferLayout viewBufferLayout;

  Renderer::MainOutput mainOutput;
  bool shouldUpdateMainOutputFromContext = false;

  Swappable<std::vector<std::function<void()>>, GFX_RENDERER_MAX_BUFFERED_FRAMES> postFrameQueue;
  Swappable<FrameReferences, GFX_RENDERER_MAX_BUFFERED_FRAMES> frameReferences;

  std::unordered_map<const View *, CachedViewDataPtr> viewCache;
  std::unordered_map<Hash128, CachedPipelinePtr> pipelineCache;

  size_t frameIndex = 0;
  const size_t maxBufferedFrames = GFX_RENDERER_MAX_BUFFERED_FRAMES;

  bool mainOutputWrittenTo = false;

  std::shared_ptr<ViewTexture> depthTexture = std::make_shared<ViewTexture>(WGPUTextureFormat_Depth24Plus, "Depth Buffer");
  std::shared_ptr<PlaceholderTexture> placeholderTexture;

  RendererImpl(Context &context) : context(context) {
    UniformBufferLayoutBuilder viewBufferLayoutBuilder;
    viewBufferLayoutBuilder.push("view", FieldTypes::Float4x4);
    viewBufferLayoutBuilder.push("proj", FieldTypes::Float4x4);
    viewBufferLayoutBuilder.push("invView", FieldTypes::Float4x4);
    viewBufferLayoutBuilder.push("invProj", FieldTypes::Float4x4);
    viewBufferLayoutBuilder.push("viewport", FieldTypes::Float4);
    viewBufferLayout = viewBufferLayoutBuilder.finalize();

    placeholderTexture = std::make_shared<PlaceholderTexture>(int2(2, 2), float4(1, 1, 1, 1));
  }

  ~RendererImpl() { releaseContextDataConditional(); }

  void initializeContextData() {
    gfxWgpuDeviceGetLimits(context.wgpuDevice, &deviceLimits);
    bindToContext(context);
  }

  virtual void releaseContextData() override {
    context.sync();
    // Flush in-flight frame resources
    for (size_t i = 0; i < maxBufferedFrames; i++) {
      swapBuffers();
    }
    pipelineCache.clear();
    viewCache.clear();
  }

  size_t alignToMinUniformOffset(size_t size) const { return alignTo(size, deviceLimits.limits.minUniformBufferOffsetAlignment); }
  size_t alignToArrayBounds(size_t size, size_t elementAlign) const { return alignTo(size, elementAlign); }

  void updateMainOutputFromContext() {
    mainOutput.format = context.getMainOutputFormat();
    mainOutput.view = context.getMainOutputTextureView();
    mainOutput.size = context.getMainOutputSize();
  }

  void renderViews(const std::vector<ViewPtr> &views, const PipelineSteps &pipelineSteps) {
    for (auto &view : views) {
      renderView(view, pipelineSteps);
    }
  }

  void renderView(ViewPtr view, const PipelineSteps &pipelineSteps) {
    View *viewPtr = view.get();

    Rect viewport;
    int2 viewSize;
    if (viewPtr->viewport) {
      viewSize = viewPtr->viewport->getSize();
      viewport = viewPtr->viewport.value();
    } else {
      viewSize = mainOutput.size;
      viewport = Rect(0, 0, viewSize.x, viewSize.y);
    }

    auto it = viewCache.find(viewPtr);
    if (it == viewCache.end()) {
      it = viewCache.insert(std::make_pair(viewPtr, std::make_shared<CachedViewData>())).first;
    }
    CachedViewData &viewData = *it->second.get();

    DrawData viewDrawData;
    viewDrawData.setParam("view", viewPtr->view);
    viewDrawData.setParam("invView", linalg::inverse(viewPtr->view));
    float4x4 projMatrix = viewData.projectionMatrix = viewPtr->getProjectionMatrix(viewSize);
    viewDrawData.setParam("proj", projMatrix);
    viewDrawData.setParam("invProj", linalg::inverse(projMatrix));

    DynamicWGPUBuffer &viewBuffer = viewData.viewBuffers.allocateBuffer(viewBufferLayout.size);
    viewBuffer.resize(context.wgpuDevice, viewBufferLayout.size, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                      "viewUniform");

    std::vector<uint8_t> stagingBuffer;
    stagingBuffer.resize(viewBufferLayout.size);
    packDrawData(stagingBuffer.data(), stagingBuffer.size(), viewBufferLayout, viewDrawData);
    wgpuQueueWriteBuffer(context.wgpuQueue, viewBuffer, 0, stagingBuffer.data(), stagingBuffer.size());

    for (auto &step : pipelineSteps) {
      std::visit(
          [&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, RenderDrawablesStep>) {
              renderDrawables(arg, view, viewport, viewBuffer);
            }
          },
          *step.get());
    }
  }

  WGPUBuffer createTransientBuffer(size_t size, WGPUBufferUsageFlags flags, const char *label = nullptr) {
    WGPUBufferDescriptor desc = {};
    desc.size = size;
    desc.label = label;
    desc.usage = flags;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(context.wgpuDevice, &desc);
    onFrameCleanup([buffer]() { wgpuBufferRelease(buffer); });
    return buffer;
  }

  void addFrameReference(std::shared_ptr<ContextData> &&contextData) {
    frameReferences(frameIndex).contextDataReferences.emplace_back(std::move(contextData));
  }

  void onFrameCleanup(std::function<void()> &&callback) { postFrameQueue(frameIndex).emplace_back(std::move(callback)); }

  void beginFrame() {
    // This registers ContextData so that releaseContextData is called when GPU resources are invalidated
    if (!isBoundToContext())
      initializeContextData();

    mainOutputWrittenTo = false;

    if (shouldUpdateMainOutputFromContext) {
      updateMainOutputFromContext();
    }

    swapBuffers();
  }

  void endFrame() {
    // FIXME Add renderer graph to handle clearing
    if (!mainOutputWrittenTo && !context.isHeadless()) {
      submitDummyRenderPass();
    }
  }

  WGPUColor clearColor{.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f};

  void submitDummyRenderPass() {
    WGPUCommandEncoderDescriptor desc = {};
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(context.wgpuDevice, &desc);

    WGPURenderPassDescriptor passDesc = {};
    passDesc.colorAttachmentCount = 1;

    WGPURenderPassColorAttachment mainAttach = {};
    mainAttach.clearValue = clearColor;
    mainAttach.view = mainOutput.view;
    mainAttach.loadOp = WGPULoadOp_Clear;
    mainAttach.storeOp = WGPUStoreOp_Store;

    passDesc.colorAttachments = &mainAttach;
    passDesc.colorAttachmentCount = 1;
    WGPURenderPassEncoder passEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDesc);
    wgpuRenderPassEncoderEnd(passEncoder);

    WGPUCommandBufferDescriptor cmdBufDesc{};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

    context.submit(cmdBuf);
  }

  void swapBuffers() {
    frameIndex = (frameIndex + 1) % maxBufferedFrames;

    frameReferences(frameIndex).clear();
    auto &postFrameQueue = this->postFrameQueue(frameIndex);
    for (auto &cb : postFrameQueue) {
      cb();
    }
    postFrameQueue.clear();

    for (auto &pair : viewCache) {
      pair.second->resetPools();
    }

    for (auto &pair : pipelineCache) {
      pair.second->resetPools();
    }
  }

  struct Bindable {
    WGPUBuffer buffer;
    UniformBufferLayout layout;
    size_t overrideSize = ~0;
    Bindable(WGPUBuffer buffer, UniformBufferLayout layout, size_t overrideSize = ~0)
        : buffer(buffer), layout(layout), overrideSize(overrideSize) {}
  };

  WGPUBindGroup createBindGroup(WGPUDevice device, WGPUBindGroupLayout layout, std::vector<Bindable> bindables) {
    WGPUBindGroupDescriptor desc = {};
    desc.label = "view";
    std::vector<WGPUBindGroupEntry> entries;

    size_t counter = 0;
    for (auto &bindable : bindables) {
      WGPUBindGroupEntry &entry = entries.emplace_back(WGPUBindGroupEntry{});
      entry.binding = counter++;
      entry.buffer = bindable.buffer;
      entry.size = (bindable.overrideSize != size_t(~0)) ? bindable.overrideSize : bindable.layout.size;
    }

    desc.entries = entries.data();
    desc.entryCount = entries.size();
    desc.layout = layout;
    return wgpuDeviceCreateBindGroup(device, &desc);
  }

  void fillInstanceBuffer(DynamicWGPUBuffer &instanceBuffer, CachedPipeline &cachedPipeline, View *view) {
    size_t alignedObjectBufferSize =
        alignToArrayBounds(cachedPipeline.objectBufferLayout.size, cachedPipeline.objectBufferLayout.maxAlignment);
    size_t numObjects = cachedPipeline.drawables.size();
    size_t instanceBufferLength = numObjects * alignedObjectBufferSize;
    instanceBuffer.resize(context.wgpuDevice, instanceBufferLength, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, "objects");

    std::vector<uint8_t> drawDataTempBuffer;
    drawDataTempBuffer.resize(instanceBufferLength);
    for (size_t i = 0; i < numObjects; i++) {
      Drawable *drawable = cachedPipeline.drawablesSorted[i].drawable;

      DrawData objectDrawData = cachedPipeline.baseDrawData;
      objectDrawData.setParam("world", drawable->transform);

      float4x4 worldInvTrans = linalg::transpose(linalg::inverse(drawable->transform));
      objectDrawData.setParam("worldInvTrans", worldInvTrans);

      // Grab draw data from material
      if (Material *material = drawable->material.get()) {
        for (auto &pair : material->parameters.basic) {
          objectDrawData.setParam(pair.first, pair.second);
        }
      }

      // Grab draw data from drawable
      for (auto &pair : drawable->parameters.basic) {
        objectDrawData.setParam(pair.first, pair.second);
      }

      // Grab draw data from features
      FeatureCallbackContext callbackContext{context, view, drawable};
      for (const Feature *feature : cachedPipeline.features) {
        for (const FeatureDrawDataFunction &drawDataGenerator : feature->drawData) {
          // TODO: Catch mismatch errors here
          drawDataGenerator(callbackContext, objectDrawData);
        }
      }

      size_t bufferOffset = alignedObjectBufferSize * i;
      size_t remainingBufferLength = instanceBufferLength - bufferOffset;

      packDrawData(drawDataTempBuffer.data() + bufferOffset, remainingBufferLength, cachedPipeline.objectBufferLayout,
                   objectDrawData);
    }

    wgpuQueueWriteBuffer(context.wgpuQueue, instanceBuffer, 0, drawDataTempBuffer.data(), drawDataTempBuffer.size());
  }

  void buildBaseObjectParameters(CachedPipeline &cachedPipeline) {
    for (const Feature *feature : cachedPipeline.features) {
      for (auto &param : feature->shaderParams) {
        cachedPipeline.baseDrawData.setParam(param.name, param.defaultValue);
      }
    }
  }

  void depthSortBackToFront(CachedPipeline &cachedPipeline, const View &view) {
    const CachedViewData &viewData = *viewCache[&view].get();

    float4x4 viewProjMatrix = linalg::mul(viewData.projectionMatrix, view.view);

    size_t numDrawables = cachedPipeline.drawables.size();
    for (size_t i = 0; i < numDrawables; i++) {
      SortableDrawable &sortable = cachedPipeline.drawablesSorted[i];
      float4 projected = mul(viewProjMatrix, mul(sortable.drawable->transform, float4(0, 0, 0, 1)));
      sortable.projectedDepth = projected.z;
    }

    auto compareBackToFront = [](const SortableDrawable &left, const SortableDrawable &right) {
      return left.projectedDepth > right.projectedDepth;
    };
    std::stable_sort(cachedPipeline.drawablesSorted.begin(), cachedPipeline.drawablesSorted.end(), compareBackToFront);
  }

  void generateTextureIds(CachedPipeline &cachedPipeline, SortableDrawable &drawable) {
    std::vector<TextureId> &textureIds = drawable.textureIds.textures;
    textureIds.reserve(cachedPipeline.textureBindingLayout.bindings.size());

    for (auto &binding : cachedPipeline.textureBindingLayout.bindings) {
      auto &drawableParams = drawable.drawable->parameters.texture;

      auto it = drawableParams.find(binding.name);
      if (it != drawableParams.end()) {
        textureIds.push_back(cachedPipeline.textureIdMap.assign(context, it->second.texture.get()));
        continue;
      }

      if (drawable.drawable->material) {
        auto &materialParams = drawable.drawable->material->parameters.texture;
        it = materialParams.find(binding.name);
        if (it != materialParams.end()) {
          textureIds.push_back(cachedPipeline.textureIdMap.assign(context, it->second.texture.get()));
          continue;
        }
      }

      textureIds.push_back(TextureId(~0));
    }
  }

  SortableDrawable createSortableDrawable(CachedPipeline &cachedPipeline, Drawable &drawable) {
    SortableDrawable sortableDrawable{};
    sortableDrawable.drawable = &drawable;
    generateTextureIds(cachedPipeline, sortableDrawable);
    sortableDrawable.key = DrawGroupKey(drawable, sortableDrawable.textureIds);

    return sortableDrawable;
  }

  void sortAndBatchDrawables(CachedPipeline &cachedPipeline, const RenderDrawablesStep &step, const View &view) {
    std::vector<SortableDrawable> &drawablesSorted = cachedPipeline.drawablesSorted;

    // Sort drawables based on mesh/texture bindings
    for (auto &drawable : cachedPipeline.drawables) {
      drawable->mesh->createContextDataConditional(context);
      addFrameReference(drawable->mesh->contextData);

      SortableDrawable sortableDrawable = createSortableDrawable(cachedPipeline, *drawable);

      auto comparison = [](const SortableDrawable &left, const SortableDrawable &right) { return left.key < right.key; };
      auto it = std::upper_bound(drawablesSorted.begin(), drawablesSorted.end(), sortableDrawable, comparison);
      drawablesSorted.insert(it, sortableDrawable);
    }

    if (step.sortMode == SortMode::BackToFront) {
      depthSortBackToFront(cachedPipeline, view);
    }

    // Creates draw call ranges based on DrawGroupKey
    if (drawablesSorted.size() > 0) {
      size_t groupStart = 0;
      DrawGroupKey currentDrawGroupKey = drawablesSorted[0].key;

      auto finishGroup = [&](size_t currentIndex) {
        size_t groupLength = currentIndex - groupStart;
        if (groupLength > 0) {
          cachedPipeline.drawGroups.emplace_back(currentDrawGroupKey, groupStart, groupLength);
        }
      };
      for (size_t i = 1; i < drawablesSorted.size(); i++) {
        DrawGroupKey drawGroupKey = drawablesSorted[i].key;
        if (drawGroupKey != currentDrawGroupKey) {
          finishGroup(i);
          groupStart = i;
          currentDrawGroupKey = drawGroupKey;
        }
      }
      finishGroup(drawablesSorted.size());
    }
  }

  void resetPipelineCacheDrawables() {
    for (auto &pair : pipelineCache) {
      pair.second->resetDrawables();
    }
  }

  WGPUBindGroup createTextureBindGroup(CachedPipeline &cachedPipeline, const TextureIds &textureIds) {
    std::vector<WGPUBindGroupEntry> entries;
    size_t bindingIndex = 0;
    for (auto &id : textureIds.textures) {
      entries.emplace_back();
      entries.emplace_back();
      WGPUBindGroupEntry &textureEntry = entries.data()[entries.size() - 2];
      textureEntry.binding = bindingIndex++;
      WGPUBindGroupEntry &samplerEntry = entries.data()[entries.size() - 1];
      samplerEntry.binding = bindingIndex++;

      TextureContextData *textureData = cachedPipeline.textureIdMap.resolve(id);
      if (textureData) {
        textureEntry.textureView = textureData->defaultView;
        samplerEntry.sampler = textureData->sampler;
      } else {
        PlaceholderTextureContextData &data = placeholderTexture->createContextDataConditional(context);
        textureEntry.textureView = data.textureView;
        samplerEntry.sampler = data.sampler;
      }
    }

    WGPUBindGroupDescriptor textureBindGroupDesc{};
    textureBindGroupDesc.layout = cachedPipeline.bindGroupLayouts[1];
    textureBindGroupDesc.entries = entries.data();
    textureBindGroupDesc.entryCount = entries.size();
    return wgpuDeviceCreateBindGroup(context.wgpuDevice, &textureBindGroupDesc);
  }

  void renderDrawables(RenderDrawablesStep &step, ViewPtr view, Rect viewport, WGPUBuffer viewBuffer) {
    if (!step.drawQueue)
      throw std::runtime_error("No draw queue assigned to drawable step");

    const DrawQueue &drawQueue = *step.drawQueue.get();

    WGPUDevice device = context.wgpuDevice;
    WGPUCommandEncoderDescriptor desc = {};
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(device, &desc);

    WGPURenderPassDescriptor passDesc = {};
    passDesc.colorAttachmentCount = 1;

    WGPURenderPassColorAttachment mainAttach = {};
    mainAttach.clearValue = clearColor;

    // FIXME Add renderer graph to handle clearing
    if (!mainOutputWrittenTo) {
      mainAttach.loadOp = WGPULoadOp_Clear;
      mainOutputWrittenTo = true;
    } else {
      mainAttach.loadOp = WGPULoadOp_Load;
    }
    mainAttach.view = mainOutput.view;
    mainAttach.storeOp = WGPUStoreOp_Store;

    WGPURenderPassDepthStencilAttachment depthAttach = {};
    depthAttach.depthClearValue = 1.0f;
    depthAttach.depthLoadOp = WGPULoadOp_Clear;
    depthAttach.depthStoreOp = WGPUStoreOp_Store;
    depthAttach.stencilLoadOp = WGPULoadOp_Undefined;
    depthAttach.stencilStoreOp = WGPUStoreOp_Undefined;
    depthAttach.view = depthTexture->update(context, viewport.getSize());

    passDesc.colorAttachments = &mainAttach;
    passDesc.colorAttachmentCount = 1;
    passDesc.depthStencilAttachment = &depthAttach;

    resetPipelineCacheDrawables();
    groupByPipeline(step, drawQueue.getDrawables());

    for (auto &pair : pipelineCache) {
      CachedPipeline &cachedPipeline = *pair.second.get();
      if (!cachedPipeline.pipeline) {
        buildPipeline(cachedPipeline);
      }

      sortAndBatchDrawables(cachedPipeline, step, *view.get());
    }

    WGPURenderPassEncoder passEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDesc);
    wgpuRenderPassEncoderSetViewport(passEncoder, (float)viewport.x, (float)viewport.y, (float)viewport.width,
                                     (float)viewport.height, 0.0f, 1.0f);

    for (auto &pair : pipelineCache) {
      CachedPipeline &cachedPipeline = *pair.second.get();
      if (cachedPipeline.drawables.empty())
        continue;

      size_t drawBufferLength =
          alignToArrayBounds(cachedPipeline.objectBufferLayout.size, cachedPipeline.objectBufferLayout.maxAlignment) *
          cachedPipeline.drawables.size();

      DynamicWGPUBuffer &instanceBuffer = cachedPipeline.instanceBufferPool.allocateBuffer(drawBufferLength);
      fillInstanceBuffer(instanceBuffer, cachedPipeline, view.get());

      auto drawGroups = cachedPipeline.drawGroups;
      std::vector<Bindable> bindables = {
          Bindable(viewBuffer, viewBufferLayout),
          Bindable(instanceBuffer, cachedPipeline.objectBufferLayout, drawBufferLength),
      };
      WGPUBindGroup bindGroup = createBindGroup(device, cachedPipeline.bindGroupLayouts[0], bindables);
      onFrameCleanup([bindGroup]() { wgpuBindGroupRelease(bindGroup); });

      wgpuRenderPassEncoderSetPipeline(passEncoder, cachedPipeline.pipeline);
      wgpuRenderPassEncoderSetBindGroup(passEncoder, 0, bindGroup, 0, nullptr);

      for (auto &drawGroup : drawGroups) {
        WGPUBindGroup textureBindGroup = createTextureBindGroup(cachedPipeline, drawGroup.key.textures);
        wgpuRenderPassEncoderSetBindGroup(passEncoder, 1, textureBindGroup, 0, nullptr);
        onFrameCleanup([textureBindGroup]() { wgpuBindGroupRelease(textureBindGroup); });

        MeshContextData *meshContextData = drawGroup.key.meshData;
        wgpuRenderPassEncoderSetVertexBuffer(passEncoder, 0, meshContextData->vertexBuffer, 0,
                                             meshContextData->vertexBufferLength);

        if (meshContextData->indexBuffer) {
          WGPUIndexFormat indexFormat = getWGPUIndexFormat(meshContextData->format.indexFormat);
          wgpuRenderPassEncoderSetIndexBuffer(passEncoder, meshContextData->indexBuffer, indexFormat, 0,
                                              meshContextData->indexBufferLength);

          wgpuRenderPassEncoderDrawIndexed(passEncoder, (uint32_t)meshContextData->numIndices, drawGroup.numInstances, 0, 0,
                                           drawGroup.startIndex);
        } else {
          wgpuRenderPassEncoderDraw(passEncoder, (uint32_t)meshContextData->numVertices, drawGroup.numInstances, 0,
                                    drawGroup.startIndex);
        }
      }
    }

    wgpuRenderPassEncoderEnd(passEncoder);

    WGPUCommandBufferDescriptor cmdBufDesc = {};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

    context.submit(cmdBuf);
  }

  void groupByPipeline(RenderDrawablesStep &step, const std::vector<DrawablePtr> &drawables) {
    // TODO: Paralellize
    std::vector<const Feature *> features;
    const std::vector<FeaturePtr> *featureSources[2] = {&step.features, nullptr};
    for (auto &drawable : drawables) {
      Drawable *drawablePtr = drawable.get();
      assert(drawablePtr->mesh);
      const Mesh &mesh = *drawablePtr->mesh.get();

      features.clear();
      for (auto &featureSource : featureSources) {
        if (!featureSource)
          continue;

        for (auto &feature : *featureSource) {
          features.push_back(feature.get());
        }
      }

      HasherXXH128 featureHasher;
      for (auto &feature : features) {
        // NOTE: Hashed by pointer since features are considered immutable & shared/ref-counted
        featureHasher(feature);
      }
      Hash128 featureHash = featureHasher.getDigest();

      HasherXXH128<HashStaticVistor> hasher;
      hasher(mesh.getFormat());
      hasher(featureHash);
      if (const Material *material = drawablePtr->material.get()) {
        hasher(*material);
      }
      Hash128 pipelineHash = hasher.getDigest();

      auto it1 = pipelineCache.find(pipelineHash);
      if (it1 == pipelineCache.end()) {
        it1 = pipelineCache.insert(std::make_pair(pipelineHash, std::make_shared<CachedPipeline>())).first;
        CachedPipeline &cachedPipeline = *it1->second.get();
        cachedPipeline.meshFormat = mesh.getFormat();
        cachedPipeline.features = features;

        buildTextureBindingLayout(cachedPipeline, drawablePtr->material.get());
      }

      CachedPipelinePtr &pipelineGroup = it1->second;
      pipelineGroup->drawables.push_back(drawablePtr);
    }
  }

  void buildTextureBindingLayout(CachedPipeline &cachedPipeline, const Material *material) {
    TextureBindingLayoutBuilder textureBindingLayoutBuilder;
    for (auto &feature : cachedPipeline.features) {
      for (auto &textureParam : feature->textureParams) {
        textureBindingLayoutBuilder.addOrUpdateSlot(textureParam.name, 0);
      }
    }

    if (material) {
      for (auto &pair : material->parameters.texture) {
        textureBindingLayoutBuilder.addOrUpdateSlot(pair.first, pair.second.defaultTexcoordBinding);
      }
    }

    cachedPipeline.textureBindingLayout = textureBindingLayoutBuilder.finalize();
  }

  shader::GeneratorOutput generateShader(const CachedPipeline &cachedPipeline) {
    using namespace shader;
    using namespace shader::blocks;

    shader::Generator generator;
    generator.meshFormat = cachedPipeline.meshFormat;

    FieldType colorFieldType(ShaderFieldBaseType::Float32, 4);
    generator.outputFields.emplace_back("color", colorFieldType);

    generator.viewBufferLayout = viewBufferLayout;
    generator.objectBufferLayout = cachedPipeline.objectBufferLayout;
    generator.textureBindingLayout = cachedPipeline.textureBindingLayout;

    std::vector<const EntryPoint *> entryPoints;
    for (auto &feature : cachedPipeline.features) {
      for (auto &entryPoint : feature->shaderEntryPoints) {
        entryPoints.push_back(&entryPoint);
      }
    }

    static const std::vector<EntryPoint> &builtinEntryPoints = []() -> const std::vector<EntryPoint> & {
      static std::vector<EntryPoint> builtin;
      builtin.emplace_back("interpolate", ProgrammableGraphicsStage::Vertex, blocks::DefaultInterpolation());
      return builtin;
    }();
    for (auto &builtinEntryPoint : builtinEntryPoints) {
      entryPoints.push_back(&builtinEntryPoint);
    }

    return generator.build(entryPoints);
  }

  void buildObjectBufferLayout(CachedPipeline &cachedPipeline) {
    UniformBufferLayoutBuilder objectBufferLayoutBuilder;
    objectBufferLayoutBuilder.push("world", FieldTypes::Float4x4);
    objectBufferLayoutBuilder.push("worldInvTrans", FieldTypes::Float4x4);
    for (const Feature *feature : cachedPipeline.features) {
      for (auto &param : feature->shaderParams) {
        objectBufferLayoutBuilder.push(param.name, param.type);
      }
    }

    cachedPipeline.objectBufferLayout = objectBufferLayoutBuilder.finalize();
  }

  FeaturePipelineState computePipelineState(const std::vector<const Feature *> &features) {
    FeaturePipelineState state{};
    for (const Feature *feature : features) {
      state = state.combine(feature->state);
    }
    return state;
  }

  // Bindgroup 0, the per-batch bound resources
  WGPUBindGroupLayout createBatchBindGroupLayout() {
    std::vector<WGPUBindGroupLayoutEntry> bindGroupLayoutEntries;

    WGPUBindGroupLayoutEntry &viewEntry = bindGroupLayoutEntries.emplace_back();
    viewEntry.binding = 0;
    viewEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
    viewEntry.buffer.type = WGPUBufferBindingType_Uniform;
    viewEntry.buffer.hasDynamicOffset = false;

    WGPUBindGroupLayoutEntry &objectEntry = bindGroupLayoutEntries.emplace_back();
    objectEntry.binding = 1;
    objectEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
    objectEntry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    objectEntry.buffer.hasDynamicOffset = false;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries.data();
    bindGroupLayoutDesc.entryCount = bindGroupLayoutEntries.size();
    return wgpuDeviceCreateBindGroupLayout(context.wgpuDevice, &bindGroupLayoutDesc);
  }

  // Bindgroup 1, bound textures
  WGPUBindGroupLayout createTextureBindGroupLayout(CachedPipeline &cachedPipeline) {
    std::vector<WGPUBindGroupLayoutEntry> bindGroupLayoutEntries;

    size_t bindingIndex = 0;
    for (auto &desc : cachedPipeline.textureBindingLayout.bindings) {
      (void)desc;

      WGPUBindGroupLayoutEntry &textureBinding = bindGroupLayoutEntries.emplace_back();
      textureBinding.binding = bindingIndex++;
      textureBinding.visibility = WGPUShaderStage_Fragment;
      textureBinding.texture.multisampled = false;
      textureBinding.texture.sampleType = WGPUTextureSampleType_Float;
      textureBinding.texture.viewDimension = WGPUTextureViewDimension_2D;

      WGPUBindGroupLayoutEntry &samplerBinding = bindGroupLayoutEntries.emplace_back();
      samplerBinding.binding = bindingIndex++;
      samplerBinding.visibility = WGPUShaderStage_Fragment;
      samplerBinding.sampler.type = WGPUSamplerBindingType_Filtering;
    }

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries.data();
    bindGroupLayoutDesc.entryCount = bindGroupLayoutEntries.size();
    return wgpuDeviceCreateBindGroupLayout(context.wgpuDevice, &bindGroupLayoutDesc);
  }

  WGPUPipelineLayout createPipelineLayout(CachedPipeline &cachedPipeline) {
    assert(!cachedPipeline.pipelineLayout);
    assert(cachedPipeline.bindGroupLayouts.empty());

    cachedPipeline.bindGroupLayouts.push_back(createBatchBindGroupLayout());
    cachedPipeline.bindGroupLayouts.push_back(createTextureBindGroupLayout(cachedPipeline));

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.bindGroupLayouts = cachedPipeline.bindGroupLayouts.data();
    pipelineLayoutDesc.bindGroupLayoutCount = cachedPipeline.bindGroupLayouts.size();
    cachedPipeline.pipelineLayout = wgpuDeviceCreatePipelineLayout(context.wgpuDevice, &pipelineLayoutDesc);
    return cachedPipeline.pipelineLayout;
  }

  struct VertexStateBuilder {
    std::vector<WGPUVertexAttribute> attributes;
    WGPUVertexBufferLayout vertexLayout = {};

    void build(WGPUVertexState &vertex, CachedPipeline &cachedPipeline) {
      size_t vertexStride = 0;
      size_t shaderLocationCounter = 0;
      for (auto &attr : cachedPipeline.meshFormat.vertexAttributes) {
        WGPUVertexAttribute &wgpuAttribute = attributes.emplace_back();
        wgpuAttribute.offset = uint64_t(vertexStride);
        wgpuAttribute.format = getWGPUVertexFormat(attr.type, attr.numComponents);
        wgpuAttribute.shaderLocation = shaderLocationCounter++;
        size_t typeSize = getVertexAttributeTypeSize(attr.type);
        vertexStride += attr.numComponents * typeSize;
      }
      vertexLayout.arrayStride = vertexStride;
      vertexLayout.attributeCount = attributes.size();
      vertexLayout.attributes = attributes.data();
      vertexLayout.stepMode = WGPUVertexStepMode::WGPUVertexStepMode_Vertex;

      vertex.bufferCount = 1;
      vertex.buffers = &vertexLayout;
      vertex.constantCount = 0;
      vertex.entryPoint = "vertex_main";
      vertex.module = cachedPipeline.shaderModule;
    }
  };

  void buildPipeline(CachedPipeline &cachedPipeline) {
    buildObjectBufferLayout(cachedPipeline);
    buildBaseObjectParameters(cachedPipeline);

    FeaturePipelineState pipelineState = computePipelineState(cachedPipeline.features);
    WGPUDevice device = context.wgpuDevice;

    shader::GeneratorOutput generatorOutput = generateShader(cachedPipeline);
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
    spdlog::info("Generated WGSL:\n {}", generatorOutput.wgslSource);

    cachedPipeline.shaderModule = wgpuDeviceCreateShaderModule(context.wgpuDevice, &moduleDesc);
    assert(cachedPipeline.shaderModule);

    WGPURenderPipelineDescriptor desc = {};
    desc.layout = createPipelineLayout(cachedPipeline);

    VertexStateBuilder vertStateBuilder;
    vertStateBuilder.build(desc.vertex, cachedPipeline);

    WGPUFragmentState fragmentState = {};
    fragmentState.entryPoint = "fragment_main";
    fragmentState.module = cachedPipeline.shaderModule;

    WGPUColorTargetState mainTarget = {};
    mainTarget.format = mainOutput.format;
    mainTarget.writeMask = pipelineState.colorWrite.value_or(WGPUColorWriteMask_All);

    WGPUBlendState blendState{};
    if (pipelineState.blend.has_value()) {
      blendState = pipelineState.blend.value();
      mainTarget.blend = &blendState;
    }

    WGPUDepthStencilState depthStencilState{};
    depthStencilState.format = depthTexture->getFormat();
    depthStencilState.depthWriteEnabled = pipelineState.depthWrite.value_or(true);
    depthStencilState.depthCompare = pipelineState.depthCompare.value_or(WGPUCompareFunction_Less);
    depthStencilState.stencilBack.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    desc.depthStencil = &depthStencilState;

    fragmentState.targets = &mainTarget;
    fragmentState.targetCount = 1;
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

    cachedPipeline.pipeline = wgpuDeviceCreateRenderPipeline(device, &desc);
    assert(cachedPipeline.pipeline);
  }
};

Renderer::Renderer(Context &context) {
  impl = std::make_shared<RendererImpl>(context);
  if (!context.isHeadless()) {
    impl->shouldUpdateMainOutputFromContext = true;
  }
  impl->initializeContextData();
}

void Renderer::render(std::vector<ViewPtr> views, const PipelineSteps &pipelineSteps) { impl->renderViews(views, pipelineSteps); }
void Renderer::render(ViewPtr view, const PipelineSteps &pipelineSteps) { impl->renderView(view, pipelineSteps); }
void Renderer::setMainOutput(const MainOutput &output) {
  impl->mainOutput = output;
  impl->shouldUpdateMainOutputFromContext = false;
}

void Renderer::beginFrame() { impl->beginFrame(); }
void Renderer::endFrame() { impl->endFrame(); }

void Renderer::cleanup() { impl->releaseContextDataConditional(); }

} // namespace gfx

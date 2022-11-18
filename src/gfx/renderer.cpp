#include "renderer.hpp"
#include "context.hpp"
#include "drawable.hpp"
#include "feature.hpp"
#include "hasherxxh128.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "mesh_utils.hpp"
#include "params.hpp"
#include "renderer_types.hpp"
#include "renderer_cache.hpp"
#include "shader/blocks.hpp"
#include "shader/entry_point.hpp"
#include "shader/fmt.hpp"
#include "shader/generator.hpp"
#include "shader/textures.hpp"
#include "shader/uniforms.hpp"
#include "texture.hpp"
#include "texture_placeholder.hpp"
#include "view.hpp"
#include "render_target.hpp"
#include "log.hpp"
#include "geom.hpp"
#include <algorithm>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

#define GFX_RENDERER_MAX_BUFFERED_FRAMES (2)

using namespace gfx::detail;

static auto logger = gfx::getLogger();

namespace gfx {

struct RendererImpl final : public ContextData {
  Context &context;
  WGPUSupportedLimits deviceLimits = {};

  UniformBufferLayout viewBufferLayout;

  ViewStack viewStack;

  Renderer::MainOutput mainOutput;
  bool shouldUpdateMainOutputFromContext = false;

  MeshPtr fullscreenQuad;

  Swappable<std::vector<std::function<void()>>, GFX_RENDERER_MAX_BUFFERED_FRAMES> postFrameQueue;
  Swappable<FrameReferences, GFX_RENDERER_MAX_BUFFERED_FRAMES> frameReferences;

  std::unordered_map<const View *, CachedViewDataPtr> viewCache;
  PipelineCache pipelineCache;

  RenderGraphEvaluator renderGraphEvaluator;

  size_t frameIndex = 0;
  const size_t maxBufferedFrames = GFX_RENDERER_MAX_BUFFERED_FRAMES;

  static constexpr WGPUTextureFormat defaultDepthTextureFormat = WGPUTextureFormat_Depth32Float;
  std::shared_ptr<Texture> depthTexture = Texture::makeRenderAttachment(defaultDepthTextureFormat, "Depth Buffer");
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

    geom::QuadGenerator quadGen;
    quadGen.optimizeForFullscreen = true;
    quadGen.generate();
    fullscreenQuad = createMesh(quadGen.vertices, std::vector<uint16_t>{});
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
    if (!mainOutput.texture) {
      mainOutput.texture = std::make_shared<Texture>();
    }

    WGPUTextureView view = context.getMainOutputTextureView();
    int2 resolution = context.getMainOutputSize();

    auto currentDesc = mainOutput.texture->getDesc();
    bool needUpdate = currentDesc.externalTexture != view || currentDesc.resolution != resolution;
    if (needUpdate) {
      mainOutput.texture
          ->init(TextureDesc{
              .format =
                  TextureFormat{
                      .type = TextureType::D2,
                      .flags = TextureFormatFlags::RenderAttachment,
                      .pixelFormat = context.getMainOutputFormat(),
                  },
              .resolution = context.getMainOutputSize(),
              .externalTexture = view,
          })
          .initWithLabel("mainOutput");
    }
  }

  void renderViews(const std::vector<ViewPtr> &views, const PipelineSteps &pipelineSteps) {
    for (auto &view : views) {
      renderView(view, pipelineSteps);
    }
  }

  void renderView(ViewPtr view, const PipelineSteps &pipelineSteps) {
    View *viewPtr = view.get();

    ViewData viewData{
        .view = *view.get(),
    };

    ViewStack::Output viewStackOutput = viewStack.getOutput();
    viewData.viewport = viewStackOutput.viewport;
    viewData.renderTarget = viewStackOutput.renderTarget;
    if (viewData.renderTarget) {
      viewData.renderTarget->resizeConditional(viewStackOutput.referenceSize);
    }

    int2 viewSize = viewStackOutput.viewport.getSize();
    if (viewSize.x == 0 || viewSize.y == 0)
      return; // Skip rendering

    auto it = viewCache.find(viewPtr);
    if (it == viewCache.end()) {
      it = viewCache.insert(std::make_pair(viewPtr, std::make_shared<CachedViewData>())).first;
    }
    CachedViewData &cachedViewData = *it->second.get();

    DrawData viewDrawData;
    viewDrawData.setParam("view", viewPtr->view);
    viewDrawData.setParam("invView", linalg::inverse(viewPtr->view));
    float4x4 projMatrix = cachedViewData.projectionMatrix = viewPtr->getProjectionMatrix(float2(viewData.viewport.getSize()));
    viewDrawData.setParam("proj", projMatrix);
    viewDrawData.setParam("invProj", linalg::inverse(projMatrix));
    viewDrawData.setParam("viewport", float4(float(viewData.viewport.x), float(viewData.viewport.y),
                                             float(viewData.viewport.width), float(viewData.viewport.height)));

    DynamicWGPUBuffer &viewBuffer = cachedViewData.viewBuffers.allocateBuffer(viewBufferLayout.size);
    viewBuffer.resize(context.wgpuDevice, viewBufferLayout.size, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                      "viewUniform");

    std::vector<uint8_t> stagingBuffer;
    stagingBuffer.resize(viewBufferLayout.size);
    packDrawData(stagingBuffer.data(), stagingBuffer.size(), viewBufferLayout, viewDrawData);
    wgpuQueueWriteBuffer(context.wgpuQueue, viewBuffer, 0, stagingBuffer.data(), stagingBuffer.size());

    viewData.viewBuffer = viewBuffer;

    RenderGraph renderGraph;
    buildRenderGraph(renderGraph, viewData, pipelineSteps);

    renderGraphEvaluator.evaluate(renderGraph, context);
  }

  void buildRenderGraph(RenderGraph &outGraph, const ViewData &viewData, const PipelineSteps &pipelineSteps) {
    for (auto &step : pipelineSteps) {
      std::visit([&](auto &&arg) { appendToRenderGraph(outGraph, viewData, arg); }, *step.get());
    }

    outGraph.populateEdges();
  }

  template <typename T> void initStepRenderTarget(RenderTargetData &renderTargetData, const ViewData &viewData, const T &step) {
    if (step.renderTarget) {
      renderTargetData.init(step.renderTarget);
    } else if (viewData.renderTarget) {
      renderTargetData.init(viewData.renderTarget);
    } else {
      // Default: output to main output
      renderTargetData.init(mainOutput, depthTexture);
    }
  }

  void collectReferencedRenderAttachments(const PipelineDrawables &pipelineDrawables, std::vector<Texture *> &outTextures) {
    // Find references to render textures
    pipelineDrawables.textureIdMap.forEachTexture([&](Texture *texture) {
      if (textureFormatFlagsContains(texture->getFormat().flags, TextureFormatFlags::RenderAttachment)) {
        outTextures.push_back(texture);
      }
    });
  }

  void preparePipelineDrawableForRenderGraph(PipelineDrawables &pipelineDrawables, RenderGraphNode &renderGraphNode,
                                             SortMode sortMode, const ViewData &viewData) {
    auto &cachedPipeline = *pipelineDrawables.cachedPipeline.get();
    if (!cachedPipeline.pipeline) {
      buildTextureBindingLayout(cachedPipeline);
      buildPipeline(cachedPipeline);
    }

    sortAndBatchDrawables(pipelineDrawables, sortMode, viewData);
    collectReferencedRenderAttachments(pipelineDrawables, renderGraphNode.readsFrom);
  }

  void applyViewport(WGPURenderPassEncoder passEncoder, const ViewData &viewData) {
    auto viewport = viewData.viewport;
    wgpuRenderPassEncoderSetViewport(passEncoder, (float)viewport.x, (float)viewport.y, (float)viewport.width,
                                     (float)viewport.height, 0.0f, 1.0f);
  }

  void appendToRenderGraph(RenderGraph &outGraph, const ViewData &viewData, const ClearStep &step) {
    auto &renderGraphNode = outGraph.nodes.emplace_back();
    auto &renderTargetData = renderGraphNode.renderTargetData;

    // Build render target references
    initStepRenderTarget(renderTargetData, viewData, step);

    renderGraphNode.setupPass = [=](WGPURenderPassDescriptor &desc) {
      for (size_t i = 0; i < desc.colorAttachmentCount; i++) {
        auto &attachment = const_cast<WGPURenderPassColorAttachment &>(desc.colorAttachments[i]);
        double4 clearValue(step.clearValues.color);
        memcpy(&attachment.clearValue.r, &clearValue.x, sizeof(double) * 4);
      }
      if (desc.depthStencilAttachment) {
        auto &attachment = const_cast<WGPURenderPassDepthStencilAttachment &>(*desc.depthStencilAttachment);
        attachment.depthClearValue = step.clearValues.depth;
        attachment.stencilClearValue = step.clearValues.stencil;
      }
    };
    renderGraphNode.body = [=, this](WGPURenderPassEncoder passEncoder) { applyViewport(passEncoder, viewData); };
  }

  void appendToRenderGraph(RenderGraph &outGraph, const ViewData &viewData, const RenderFullscreenStep &step) {
    auto &renderGraphNode = outGraph.nodes.emplace_back();
    auto &renderTargetData = renderGraphNode.renderTargetData;

    // Build render target references
    initStepRenderTarget(renderTargetData, viewData, step);

    // Derive format layout
    RenderTargetLayout renderTargetLayout = renderGraphNode.renderTargetData.getLayout();

    DrawablePtr drawable = std::make_shared<Drawable>(fullscreenQuad);
    drawable->parameters = step.parameters;
    onFrameCleanup([drawable = drawable]() mutable { drawable.reset(); });

    DrawableGrouper grouper(renderTargetLayout);
    Hash128 sharedHash = grouper.generateSharedHash(renderTargetLayout);
    auto groupedDrawable = grouper.groupByPipeline(pipelineCache, sharedHash, step.features,
                                                   std::vector<const MaterialParameters *>{&step.parameters}, *drawable.get());

    PipelineDrawables pipelineDrawables;
    pipelineDrawables.cachedPipeline = groupedDrawable.cachedPipeline;
    pipelineDrawables.drawables.push_back(drawable.get());

    preparePipelineDrawableForRenderGraph(pipelineDrawables, renderGraphNode, SortMode::Queue, viewData);

    renderGraphNode.body = [=, this](WGPURenderPassEncoder passEncoder) {
      applyViewport(passEncoder, viewData);
      renderPipelineDrawables(passEncoder, pipelineDrawables, viewData);
    };
  }

  void appendToRenderGraph(RenderGraph &outGraph, const ViewData &viewData, const RenderDrawablesStep &step) {
    if (!step.drawQueue)
      throw std::runtime_error("No draw queue assigned to drawable step");

    auto &renderGraphNode = outGraph.nodes.emplace_back();
    auto &renderTargetData = renderGraphNode.renderTargetData;

    renderGraphNode.forceDepthClear = step.forceDepthClear;

    // Build render target references
    initStepRenderTarget(renderTargetData, viewData, step);

    // Derive format layout
    RenderTargetLayout renderTargetLayout = renderGraphNode.renderTargetData.getLayout();

    const DrawQueue &drawQueue = *step.drawQueue.get();

    // Sort all draw commands into their respective pipeline
    std::shared_ptr<PipelineDrawableCache> pipelineDrawableCache = std::make_shared<PipelineDrawableCache>();

    // Sort draw commands into pipeline
    DrawableGrouper grouper(renderTargetLayout);
    Hash128 sharedHash = grouper.generateSharedHash(renderTargetLayout);
    static const std::vector<const MaterialParameters *> sharedParameters{}; // empty
    grouper.groupByPipeline(*pipelineDrawableCache.get(), pipelineCache, sharedHash, step.features, sharedParameters,
                            drawQueue.getDrawables());

    // Sort draw commands, build pipelines
    for (auto &pair : pipelineDrawableCache->map) {
      preparePipelineDrawableForRenderGraph(pair.second, renderGraphNode, step.sortMode, viewData);
    }

    renderGraphNode.body = [=, this](WGPURenderPassEncoder passEncoder) {
      applyViewport(passEncoder, viewData);
      for (auto &pair : pipelineDrawableCache->map) {
        renderPipelineDrawables(passEncoder, pair.second, viewData);
      }
    };
  }

  void renderPipelineDrawables(WGPURenderPassEncoder passEncoder, const PipelineDrawables &pipelineDrawables,
                               const ViewData &viewData) {
    CachedPipeline &cachedPipeline = *pipelineDrawables.cachedPipeline.get();
    if (pipelineDrawables.drawablesSorted.empty())
      return;

    // Bytes between object buffer entries
    size_t objectBufferStride = cachedPipeline.objectBufferLayout.getArrayStride();
    size_t objectBufferLength = objectBufferStride * pipelineDrawables.drawablesSorted.size();

    DynamicWGPUBuffer &objectBuffer = cachedPipeline.instanceBufferPool.allocateBuffer(objectBufferLength);
    fillInstanceBuffer(objectBuffer, pipelineDrawables, viewData.view);

    auto drawGroups = pipelineDrawables.drawGroups;

    wgpuRenderPassEncoderSetPipeline(passEncoder, cachedPipeline.pipeline);

    std::vector<Bindable> pipelineBindings = {
        Bindable(viewData.viewBuffer, viewBufferLayout),
    };

    auto objectBinding = Bindable(objectBuffer, cachedPipeline.objectBufferLayout, objectBufferLength);

    // Pipeline-shared bind group
    WGPUBindGroup pipelineBindGroup = createBindGroup(cachedPipeline.bindGroupLayouts[0], pipelineBindings, "pipelineShared");
    onFrameCleanup([pipelineBindGroup]() { wgpuBindGroupRelease(pipelineBindGroup); });
    wgpuRenderPassEncoderSetBindGroup(passEncoder, 0, pipelineBindGroup, 0, nullptr);

    for (auto &drawGroup : pipelineDrawables.drawGroups) {
      // Some platforms don't support instanceOffset, so we bind the object buffer at an offset to render different
      size_t objectBufferOffset = drawGroup.startIndex * objectBufferStride;
      size_t objectBufferRemainingSize = objectBufferLength - objectBufferOffset;
      objectBinding.offset = objectBufferOffset;
      objectBinding.overrideSize = objectBufferRemainingSize;

      if (drawGroup.key.clipRect) {
        auto &clipRect = drawGroup.key.clipRect.value();
        wgpuRenderPassEncoderSetScissorRect(passEncoder, clipRect.x, clipRect.y, clipRect.z - clipRect.x,
                                            clipRect.w - clipRect.y);
      } else {
        wgpuRenderPassEncoderSetScissorRect(passEncoder, viewData.viewport.x, viewData.viewport.y, viewData.viewport.width,
                                            viewData.viewport.height);
      }

      WGPUBindGroup batchBindGroup = createBatchBindGroup(pipelineDrawables, objectBinding, drawGroup.key.textures);
      wgpuRenderPassEncoderSetBindGroup(passEncoder, 1, batchBindGroup, 0, nullptr);
      onFrameCleanup([batchBindGroup]() { wgpuBindGroupRelease(batchBindGroup); });

      MeshContextData *meshContextData = drawGroup.key.meshData;
      assert(meshContextData->vertexBuffer);
      wgpuRenderPassEncoderSetVertexBuffer(passEncoder, 0, meshContextData->vertexBuffer, 0, meshContextData->vertexBufferLength);

      if (meshContextData->indexBuffer) {
        WGPUIndexFormat indexFormat = getWGPUIndexFormat(meshContextData->format.indexFormat);
        wgpuRenderPassEncoderSetIndexBuffer(passEncoder, meshContextData->indexBuffer, indexFormat, 0,
                                            meshContextData->indexBufferLength);

        wgpuRenderPassEncoderDrawIndexed(passEncoder, (uint32_t)meshContextData->numIndices, drawGroup.numInstances, 0, 0, 0);
      } else {
        wgpuRenderPassEncoderDraw(passEncoder, (uint32_t)meshContextData->numVertices, drawGroup.numInstances, 0, 0);
      }
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

    swapBuffers();

    if (shouldUpdateMainOutputFromContext) {
      updateMainOutputFromContext();
    }

    renderGraphEvaluator.reset();

    auto mainOutputResolution = mainOutput.texture->getResolution();
    viewStack.push(ViewStack::Item{
        .viewport = Rect(mainOutputResolution),
        .referenceSize = mainOutputResolution,
    });

    resizeMainRenderAttachments(mainOutputResolution);
  }

  void endFrame() {
    viewStack.pop(); // Main output
    viewStack.reset();

    ensureMainOutputCleared();
  }

  void resizeMainRenderAttachments(int2 referenceSize) { depthTexture->initWithResolution(referenceSize); }

  void ensureMainOutputCleared() {
    if (!renderGraphEvaluator.isWrittenTo(mainOutput.texture)) {
      WGPUCommandEncoderDescriptor desc = {};
      WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(context.wgpuDevice, &desc);

      // Make sure primary output is cleared if no render steps are present
      renderGraphEvaluator.clearColorTexture(context, mainOutput.texture, commandEncoder);

      WGPUCommandBufferDescriptor cmdBufDesc{};
      WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

      context.submit(cmdBuf);
    }
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

    for (auto &pair : pipelineCache.map) {
      pair.second->resetPools();
    }
  }

  WGPUBindGroup createBindGroup(WGPUBindGroupLayout layout, const std::vector<Bindable> &bindables, const char *label = nullptr) {
    WGPUBindGroupDescriptor desc = {};
    desc.label = label;
    std::vector<WGPUBindGroupEntry> entries;

    size_t counter = 0;
    for (auto &bindable : bindables) {
      entries.emplace_back(bindable.toBindGroupEntry(counter));
    }

    desc.entries = entries.data();
    desc.entryCount = entries.size();
    desc.layout = layout;

    WGPUBindGroup result = wgpuDeviceCreateBindGroup(context.wgpuDevice, &desc);
    assert(result);

    return result;
  }

  WGPUBindGroup createBatchBindGroup(const PipelineDrawables &pipelineDrawables, const Bindable &objectBinding,
                                     const TextureIds &textureIds) {
    std::vector<WGPUBindGroupEntry> entries;

    size_t bindingIndex = 0;

    entries.emplace_back(objectBinding.toBindGroupEntry(bindingIndex));

    for (auto &id : textureIds.textures) {
      entries.emplace_back();
      entries.emplace_back();
      WGPUBindGroupEntry &textureEntry = entries.data()[entries.size() - 2];
      textureEntry.binding = bindingIndex++;
      WGPUBindGroupEntry &samplerEntry = entries.data()[entries.size() - 1];
      samplerEntry.binding = bindingIndex++;

      TextureContextData *textureData = pipelineDrawables.textureIdMap.resolve(id);
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
    textureBindGroupDesc.layout = pipelineDrawables.cachedPipeline->bindGroupLayouts[1];
    textureBindGroupDesc.entries = entries.data();
    textureBindGroupDesc.entryCount = entries.size();
    textureBindGroupDesc.label = "batchShared";

    WGPUBindGroup result = wgpuDeviceCreateBindGroup(context.wgpuDevice, &textureBindGroupDesc);
    assert(result);

    return result;
  }

  void fillInstanceBuffer(DynamicWGPUBuffer &instanceBuffer, const PipelineDrawables &pipelineDrawables, View &view) {
    auto &cachedPipeline = *pipelineDrawables.cachedPipeline.get();
    size_t numObjects = pipelineDrawables.drawablesSorted.size();
    size_t stride = cachedPipeline.objectBufferLayout.getArrayStride();

    size_t instanceBufferLength = numObjects * stride;
    instanceBuffer.resize(context.wgpuDevice, instanceBufferLength, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, "objects");

    std::vector<uint8_t> drawDataTempBuffer;
    drawDataTempBuffer.resize(instanceBufferLength);
    for (size_t i = 0; i < numObjects; i++) {
      const Drawable *drawable = pipelineDrawables.drawablesSorted[i].drawable;

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
      FeatureCallbackContext callbackContext{context, &view, drawable};
      for (const Feature *feature : cachedPipeline.features) {
        for (const FeatureDrawDataFunction &drawDataGenerator : feature->drawData) {
          // TODO: Catch mismatch errors here
          drawDataGenerator(callbackContext, objectDrawData);
        }
      }

      size_t bufferOffset = stride * i;
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

  void depthSortBackToFront(PipelineDrawables &pipelineDrawables, const View &view) {
    const CachedViewData &viewData = *viewCache[&view].get();

    float4x4 viewProjMatrix = linalg::mul(viewData.projectionMatrix, view.view);

    size_t numDrawables = pipelineDrawables.drawablesSorted.size();
    for (size_t i = 0; i < numDrawables; i++) {
      SortableDrawable &sortable = pipelineDrawables.drawablesSorted[i];
      float4 projected = mul(viewProjMatrix, mul(sortable.drawable->transform, float4(0, 0, 0, 1)));
      sortable.projectedDepth = projected.z;
    }

    auto compareBackToFront = [](const SortableDrawable &left, const SortableDrawable &right) {
      return left.projectedDepth > right.projectedDepth;
    };
    std::stable_sort(pipelineDrawables.drawablesSorted.begin(), pipelineDrawables.drawablesSorted.end(), compareBackToFront);
  }

  void generateTextureIds(PipelineDrawables &pipelineDrawables, SortableDrawable &drawable) {
    auto &cachedPipeline = *pipelineDrawables.cachedPipeline.get();

    std::vector<TextureId> &textureIds = drawable.textureIds.textures;
    textureIds.reserve(cachedPipeline.textureBindingLayout.bindings.size());

    for (auto &binding : cachedPipeline.textureBindingLayout.bindings) {
      auto &drawableParams = drawable.drawable->parameters.texture;

      auto tryAssignTexture = [&](const std::map<std::string, TextureParameter> &params, const std::string &name) -> bool {
        auto it = params.find(binding.name);
        if (it != params.end()) {
          const TexturePtr &texture = it->second.texture;
          TextureContextData &contextData = texture->createContextDataConditional(context);
          if (!contextData.defaultView)
            return false; // Invalid texture

          textureIds.push_back(pipelineDrawables.textureIdMap.assign(texture.get()));
          return true;
        }
        return false;
      };

      if (tryAssignTexture(drawableParams, binding.name))
        continue;

      if (drawable.drawable->material) {
        if (tryAssignTexture(drawable.drawable->material->parameters.texture, binding.name))
          continue;
      }

      // Mark as unassigned texture
      textureIds.push_back(TextureId(~0));
    }
  }

  SortableDrawable createSortableDrawable(PipelineDrawables &pipelineDrawables, const Drawable &drawable) {
    SortableDrawable sortableDrawable{};
    sortableDrawable.drawable = &drawable;
    generateTextureIds(pipelineDrawables, sortableDrawable);
    sortableDrawable.key = DrawGroupKey(drawable, sortableDrawable.textureIds);

    return sortableDrawable;
  }

  void sortAndBatchDrawables(PipelineDrawables &pipelineDrawables, SortMode sortMode, const ViewData &viewData) {
    auto &view = viewData.view;
    std::vector<SortableDrawable> &drawablesSorted = pipelineDrawables.drawablesSorted;

    // Sort drawables based on mesh/texture bindings
    for (auto &drawable : pipelineDrawables.drawables) {
      drawable->mesh->createContextDataConditional(context);

      // Filter out empty meshes here
      if (drawable->mesh->contextData->numVertices == 0)
        continue;

      addFrameReference(drawable->mesh->contextData);

      SortableDrawable sortableDrawable = createSortableDrawable(pipelineDrawables, *drawable);

      if (sortMode == SortMode::Queue) {
        drawablesSorted.push_back(sortableDrawable);
      } else {
        auto comparison = [](const SortableDrawable &left, const SortableDrawable &right) { return left.key < right.key; };
        auto it = std::upper_bound(drawablesSorted.begin(), drawablesSorted.end(), sortableDrawable, comparison);
        drawablesSorted.insert(it, sortableDrawable);
      }
    }

    if (sortMode == SortMode::BackToFront) {
      depthSortBackToFront(pipelineDrawables, view);
    }

    // Creates draw call ranges based on DrawGroupKey
    if (drawablesSorted.size() > 0) {
      size_t groupStart = 0;
      DrawGroupKey currentDrawGroupKey = drawablesSorted[0].key;

      auto finishGroup = [&](size_t currentIndex) {
        size_t groupLength = currentIndex - groupStart;
        if (groupLength > 0) {
          pipelineDrawables.drawGroups.emplace_back(currentDrawGroupKey, groupStart, groupLength);
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

  WGPUBindGroup createTextureBindGroup(const PipelineDrawables &pipelineDrawables, const TextureIds &textureIds) {
    auto &cachedPipeline = *pipelineDrawables.cachedPipeline.get();

    std::vector<WGPUBindGroupEntry> entries;
    size_t bindingIndex = 0;
    for (auto &id : textureIds.textures) {
      entries.emplace_back();
      entries.emplace_back();
      WGPUBindGroupEntry &textureEntry = entries.data()[entries.size() - 2];
      textureEntry.binding = bindingIndex++;
      WGPUBindGroupEntry &samplerEntry = entries.data()[entries.size() - 1];
      samplerEntry.binding = bindingIndex++;

      TextureContextData *textureData = pipelineDrawables.textureIdMap.resolve(id);
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

    WGPUBindGroup result = wgpuDeviceCreateBindGroup(context.wgpuDevice, &textureBindGroupDesc);
    assert(result);

    return result;
  }

  void buildTextureBindingLayout(CachedPipeline &cachedPipeline) {
    TextureBindingLayoutBuilder textureBindingLayoutBuilder;
    for (auto &feature : cachedPipeline.features) {
      for (auto &textureParam : feature->textureParams) {
        textureBindingLayoutBuilder.addOrUpdateSlot(textureParam.name, 0);
      }
    }

    // Update default texture coordinates based on material
    for (auto &pair : cachedPipeline.materialTextureBindings) {
      textureBindingLayoutBuilder.tryUpdateSlot(pair.first, pair.second.defaultTexcoordBinding);
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

    auto &layout = cachedPipeline.objectBufferLayout = objectBufferLayoutBuilder.finalize();

    // Align the struct ot storage buffer offset requriements
    layout.maxAlignment = alignTo(layout.maxAlignment, deviceLimits.limits.minStorageBufferOffsetAlignment);
  }

  FeaturePipelineState computePipelineState(const std::vector<const Feature *> &features) {
    FeaturePipelineState state{};
    for (const Feature *feature : features) {
      state = state.combine(feature->state);
    }
    return state;
  }

  // Bindgroup 0, the per-batch bound resources
  WgpuHandle<WGPUBindGroupLayout> createPipelineBindGroupLayout(CachedPipeline &cachedPipeline) {
    std::vector<WGPUBindGroupLayoutEntry> bindGroupLayoutEntries;

    WGPUBindGroupLayoutEntry &viewEntry = bindGroupLayoutEntries.emplace_back();
    viewEntry.binding = 0;
    viewEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
    viewEntry.buffer.type = WGPUBufferBindingType_Uniform;
    viewEntry.buffer.hasDynamicOffset = false;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.label = "pipelineShared";
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries.data();
    bindGroupLayoutDesc.entryCount = bindGroupLayoutEntries.size();
    return toWgpuHandle(wgpuDeviceCreateBindGroupLayout(context.wgpuDevice, &bindGroupLayoutDesc));
  }

  // Bindgroup 1, per batch constants + textures
  WgpuHandle<WGPUBindGroupLayout> createBatchBindGroupLayout(CachedPipeline &cachedPipeline) {
    std::vector<WGPUBindGroupLayoutEntry> bindGroupLayoutEntries;

    size_t bindingIndex = 0;

    // Instance data
    WGPUBindGroupLayoutEntry &constantsEntry = bindGroupLayoutEntries.emplace_back();
    constantsEntry.binding = bindingIndex++;
    constantsEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
    constantsEntry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    constantsEntry.buffer.hasDynamicOffset = false;

    // Interleaved Texture&Sampler bindings (1 each per texture slot)
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
    bindGroupLayoutDesc.label = "textures";
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries.data();
    bindGroupLayoutDesc.entryCount = bindGroupLayoutEntries.size();
    return toWgpuHandle(wgpuDeviceCreateBindGroupLayout(context.wgpuDevice, &bindGroupLayoutDesc));
  }

  WGPUPipelineLayout createPipelineLayout(CachedPipeline &cachedPipeline) {
    assert(!cachedPipeline.pipelineLayout);
    assert(cachedPipeline.bindGroupLayouts.empty());

    cachedPipeline.bindGroupLayouts.push_back(createPipelineBindGroupLayout(cachedPipeline));
    cachedPipeline.bindGroupLayouts.push_back(createBatchBindGroupLayout(cachedPipeline));

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout *>(cachedPipeline.bindGroupLayouts.data());
    pipelineLayoutDesc.bindGroupLayoutCount = cachedPipeline.bindGroupLayouts.size();
    cachedPipeline.pipelineLayout.reset(wgpuDeviceCreatePipelineLayout(context.wgpuDevice, &pipelineLayoutDesc));
    return cachedPipeline.pipelineLayout;
  }

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
    SPDLOG_LOGGER_DEBUG(logger, "Generated WGSL:\n {}", generatorOutput.wgslSource);

    cachedPipeline.shaderModule.reset(wgpuDeviceCreateShaderModule(context.wgpuDevice, &moduleDesc));
    assert(cachedPipeline.shaderModule);

    WGPURenderPipelineDescriptor desc = {};
    desc.layout = createPipelineLayout(cachedPipeline);

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

    cachedPipeline.pipeline.reset(wgpuDeviceCreateRenderPipeline(device, &desc));
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

Context &Renderer::getContext() { return impl->context; }

void Renderer::render(std::vector<ViewPtr> views, const PipelineSteps &pipelineSteps) { impl->renderViews(views, pipelineSteps); }
void Renderer::render(ViewPtr view, const PipelineSteps &pipelineSteps) { impl->renderView(view, pipelineSteps); }
void Renderer::setMainOutput(const MainOutput &output) {
  impl->mainOutput = output;
  impl->shouldUpdateMainOutputFromContext = false;
}

ViewStack &Renderer::getViewStack() { return impl->viewStack; }

void Renderer::beginFrame() { impl->beginFrame(); }
void Renderer::endFrame() { impl->endFrame(); }

void Renderer::cleanup() { impl->releaseContextDataConditional(); }

} // namespace gfx

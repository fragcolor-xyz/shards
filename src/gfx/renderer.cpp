#include "renderer.hpp"
#include "context.hpp"
#include "drawable.hpp"
#include "feature.hpp"
#include "hasherxxh128.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "mesh_utils.hpp"
#include "params.hpp"
#include "pipeline_builder.hpp"
#include "renderer_types.hpp"
#include "renderer_cache.hpp"
#include "render_graph.hpp"
#include "shader/blocks.hpp"
#include "shader/entry_point.hpp"
#include "shader/fmt.hpp"
#include "shader/generator.hpp"
#include "shader/textures.hpp"
#include "shader/uniforms.hpp"
#include "steps/defaults.hpp"
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

namespace gfx {

static auto logger = gfx::getLogger();

static auto defaultRenderStepOutput = steps::getDefaultRenderStepOutput();

static MeshPtr fullscreenQuad = []() {
  geom::QuadGenerator quadGen;
  quadGen.optimizeForFullscreen = true;
  quadGen.generate();
  return createMesh(quadGen.vertices, std::vector<uint16_t>{});
}();

// Bound to unset texture parameter slots
static std::shared_ptr<PlaceholderTexture> placeholderTexture = []() {
  return std::make_shared<PlaceholderTexture>(int2(2, 2), float4(1, 1, 1, 1));
}();

struct RendererImpl final : public ContextData {
  Context &context;
  WGPUSupportedLimits deviceLimits = {};

  ViewStack viewStack;

  Renderer::MainOutput mainOutput;
  bool shouldUpdateMainOutputFromContext = false;

  Swappable<std::vector<std::function<void()>>, GFX_RENDERER_MAX_BUFFERED_FRAMES> postFrameQueue;
  Swappable<FrameReferences, GFX_RENDERER_MAX_BUFFERED_FRAMES> frameReferences;

  std::unordered_map<const Drawable *, CachedDrawablePtr> drawableCache;
  std::unordered_map<const View *, CachedViewDataPtr> viewCache;
  RenderGraphCache renderGraphCache;
  PipelineCache pipelineCache;

  RenderGraphEvaluator renderGraphEvaluator;

  const size_t maxBufferedFrames = GFX_RENDERER_MAX_BUFFERED_FRAMES;

  // Within the range [0, maxBufferedFrames)
  size_t frameIndex = 0;

  // Increments forever
  size_t frameCounter = 0;

  RendererImpl(Context &context) : context(context) {}

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

    renderGraphCache.clear();
    pipelineCache.clear();
    viewCache.clear();
    drawableCache.clear();
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
                      .flags = TextureFormatFlags::RenderAttachment | TextureFormatFlags::NoTextureBinding,
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

  CachedView &getCachedView(const ViewPtr &view) { return *getSharedCacheEntry<CachedView>(viewCache, view.get()).get(); }

  CachedDrawable &getCachedDrawable(const Drawable *drawable) {
    return *getSharedCacheEntry<CachedDrawable>(drawableCache, drawable).get();
  }

  void setViewData(DrawData &outDrawData, const ViewData &viewData) {
    outDrawData.setParam("view", viewData.view.view);
    outDrawData.setParam("invView", viewData.cachedView.invViewTransform);
    outDrawData.setParam("proj", viewData.cachedView.projectionTransform);
    outDrawData.setParam("invProj", viewData.cachedView.invProjectionTransform);
    outDrawData.setParam("viewport", float4(float(viewData.viewport.x), float(viewData.viewport.y),
                                            float(viewData.viewport.width), float(viewData.viewport.height)));
  }

  std::vector<TexturePtr> renderGraphOutputs;
  void renderView(ViewPtr view, const PipelineSteps &pipelineSteps) {
    ViewData viewData{
        .view = *view.get(),
        .cachedView = getCachedView(view),
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

    viewData.cachedView.touchWithNewTransform(view->view, view->getProjectionMatrix(float2(viewSize)), frameCounter);

    // Match with attached outputs in buildRenderGraph
    renderGraphOutputs.clear();
    if (viewData.renderTarget) {
      for (auto &attachment : viewData.renderTarget->attachments) {
        renderGraphOutputs.push_back(attachment.second);
      }
    } else {
      renderGraphOutputs.push_back(mainOutput.texture);
    }

    const RenderGraph &renderGraph = getOrBuildRenderGraph(viewData, pipelineSteps, viewStackOutput.referenceSize);
    renderGraphEvaluator.evaluate(renderGraph, renderGraphOutputs, context, frameCounter);
  }

  void buildRenderGraph(const ViewData &viewData, const PipelineSteps &pipelineSteps, int2 referenceOutputSize,
                        CachedRenderGraph &out) {

    RenderGraphBuilder builder;

    builder.setReferenceOutputSize(referenceOutputSize);

    size_t index{};
    for (auto &step : pipelineSteps) {
      std::visit([&](auto &&arg) { allocateNodeEdges(builder, index++, arg); }, *step.get());
    }

    // Attach outputs
    if (viewData.renderTarget) {
      for (auto &attachment : viewData.renderTarget->attachments)
        builder.attachOutput(attachment.first, attachment.second);
    } else {
      builder.attachOutput("color", mainOutput.texture);
    }

    builder.updateNodeLayouts();

    out.renderGraph = builder.finalize();

    index = 0;
    for (auto &step : pipelineSteps) {
      std::visit([&](auto &&arg) { setupRenderGraphNode(out, index, viewData, arg); }, *step.get());
      index++;
    }
  }

  const RenderGraph &getOrBuildRenderGraph(const ViewData &viewData, const PipelineSteps &pipelineSteps,
                                           int2 referenceOutputSize) {
    HasherXXH128<HashStaticVistor> hasher;
    for (auto &step : pipelineSteps) {
      // NOTE: Hashed by pointer since steps are considered immutable & shared/ref-counted
      hasher(step.get());
    }
    hasher(referenceOutputSize);
    if (viewData.renderTarget) {
      for (auto &attachment : viewData.renderTarget->attachments) {
        hasher(attachment.first);
        hasher(attachment.second->getFormat().pixelFormat);
      }
    } else {
      hasher("color");
      hasher(mainOutput.texture->getFormat().pixelFormat);
    }
    Hash128 renderGraphHash = hasher.getDigest();

    auto &cachedRenderGraph = getCacheEntry(renderGraphCache.map, renderGraphHash, [&](const Hash128 &key) {
      auto result = std::make_shared<CachedRenderGraph>();
      buildRenderGraph(viewData, pipelineSteps, referenceOutputSize, *result.get());
      return result;
    });

    return cachedRenderGraph->renderGraph;
  }

  void allocateNodeEdges(detail::RenderGraphBuilder &builder, size_t index, const ClearStep &step) {
    builder.allocateOutputs(index, step.output ? step.output.value() : defaultRenderStepOutput);
  }
  void setupRenderGraphNode(CachedRenderGraph &out, size_t index, const ViewData &viewData, const ClearStep &step) {
    auto &node = out.getNode(index);
    node.setupPass = [=](WGPURenderPassDescriptor &desc) {
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
  }

  void allocateNodeEdges(detail::RenderGraphBuilder &builder, size_t index, const RenderDrawablesStep &step) {
    builder.allocateOutputs(index, step.output ? step.output.value() : defaultRenderStepOutput);
  }
  void setupRenderGraphNode(CachedRenderGraph &out, size_t index, const ViewData &viewData, const RenderDrawablesStep &step) {
    auto &node = out.getNode(index);

    if (!step.drawQueue)
      throw std::runtime_error("No draw queue assigned to drawable step");

    // Sort all draw commands into their respective pipeline
    std::shared_ptr<PipelineDrawableCache> pipelineDrawableCache = std::make_shared<PipelineDrawableCache>();

    // Sort draw commands into pipeline
    std::shared_ptr<DrawableGrouper> grouper = std::make_shared<DrawableGrouper>(node.renderTargetLayout);
    Hash128 sharedHash = grouper->generateSharedHash();

    node.body = [=, this, drawQueue = step.drawQueue](EvaluateNodeContext &ctx) {
      pipelineDrawableCache->clear();

      static const std::vector<const MaterialParameters *> sharedParameters{};
      grouper->groupByPipeline(*pipelineDrawableCache.get(), pipelineCache, sharedHash, step.features, sharedParameters,
                               drawQueue->getDrawables());

      // Sort draw commands, build pipelines
      for (auto &pair : pipelineDrawableCache->map) {
        touchCacheItemPtr(pair.second.cachedPipeline, frameCounter);
        preparePipelineDrawableForRenderGraph(pair.second, ctx.node, step.sortMode, viewData);
      }

      applyViewport(ctx.encoder, viewData);
      for (auto &pair : pipelineDrawableCache->map) {
        renderPipelineDrawables(ctx.encoder, pair.second, viewData);
      }

      // Release references
      pipelineDrawableCache->clear();
    };
  }

  void allocateNodeEdges(detail::RenderGraphBuilder &builder, size_t index, const RenderFullscreenStep &step) {
    builder.allocateInputs(index, step.inputs);

    bool overwriteTargets = step.overlay ? false : true;
    builder.allocateOutputs(index, step.output ? step.output.value() : defaultRenderStepOutput, overwriteTargets);
  }
  void setupRenderGraphNode(CachedRenderGraph &out, size_t index, const ViewData &viewData, const RenderFullscreenStep &step) {
    RenderGraphNode &node = out.getNode(index);

    DrawablePtr drawable = std::make_shared<Drawable>(fullscreenQuad);
    drawable->parameters = step.parameters;

    // Setup node outputs as texture slots
    FeaturePtr baseFeature = std::make_shared<Feature>();
    for (auto &frameIndex : node.readsFrom) {
      auto &frame = out.renderGraph.frames[frameIndex];
      baseFeature->textureParams.emplace_back(frame.name);
    }
    drawable->features.push_back(baseFeature);

    node.body = [=](EvaluateNodeContext &ctx) {
      for (auto &frameIndex : ctx.node.readsFrom) {
        auto &texture = ctx.evaluator.getTexture(frameIndex);
        auto &frame = ctx.graph.frames[frameIndex];
        drawable->parameters.set(frame.name, texture);
      }

      DrawableGrouper grouper(ctx.node.renderTargetLayout);
      Hash128 sharedHash = grouper.generateSharedHash();
      auto groupedDrawable = grouper.groupByPipeline(pipelineCache, sharedHash, step.features,
                                                     std::vector<const MaterialParameters *>{&step.parameters}, *drawable.get());
      touchCacheItemPtr(groupedDrawable.cachedPipeline, frameCounter);

      PipelineDrawables pipelineDrawables;
      pipelineDrawables.cachedPipeline = groupedDrawable.cachedPipeline;
      pipelineDrawables.drawables.push_back(drawable.get());

      preparePipelineDrawableForRenderGraph(pipelineDrawables, ctx.node, SortMode::Queue, viewData);

      applyViewport(ctx.encoder, viewData);
      renderPipelineDrawables(ctx.encoder, pipelineDrawables, viewData);
    };
  }

  void preparePipelineDrawableForRenderGraph(PipelineDrawables &pipelineDrawables, const RenderGraphNode &renderGraphNode,
                                             SortMode sortMode, const ViewData &viewData) {
    auto &cachedPipeline = *pipelineDrawables.cachedPipeline.get();
    if (!cachedPipeline.pipeline) {
      buildPipeline(cachedPipeline);
    }

    sortAndBatchDrawables(pipelineDrawables, sortMode, viewData);
  }

  void applyViewport(WGPURenderPassEncoder passEncoder, const ViewData &viewData) {
    auto viewport = viewData.viewport;
    wgpuRenderPassEncoderSetViewport(passEncoder, (float)viewport.x, (float)viewport.y, (float)viewport.width,
                                     (float)viewport.height, 0.0f, 1.0f);
  }

  void setupFixedSizeBuffers(CachedPipeline &cachedPipeline) {
    std::vector<WGPUBindGroupEntry> viewBindGroupEntries;

    // Setup view bindings since they don't change in size
    cachedPipeline.viewBuffers.resize(cachedPipeline.viewBuffersBindings.size());
    for (auto &binding : cachedPipeline.viewBuffersBindings) {
      WGPUBufferDescriptor desc{};
      desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
      desc.size = binding.layout.size;

      WGPUBuffer buffer = wgpuDeviceCreateBuffer(context.wgpuDevice, &desc);
      cachedPipeline.viewBuffers[binding.index].reset(buffer);

      auto &entry = viewBindGroupEntries.emplace_back();
      entry.binding = binding.index;
      entry.size = binding.layout.size;
      entry.buffer = buffer;
    }

    // Determine number of draw bindings
    size_t numDrawBindings{};
    for (auto &binding : cachedPipeline.drawBufferBindings) {
      numDrawBindings = std::max(numDrawBindings, binding.index + 1);
    }
    for (auto &binding : cachedPipeline.textureBindingLayout.bindings) {
      numDrawBindings = std::max(numDrawBindings, binding.binding + 1);
      numDrawBindings = std::max(numDrawBindings, binding.defaultSamplerBinding + 1);
    }
    cachedPipeline.drawBindGroupEntries.resize(numDrawBindings);

    // For each draw buffer, keep track of buffer size / stride
    cachedPipeline.drawBufferInstanceDescriptions.resize(cachedPipeline.drawBufferBindings.size());

    WGPUBindGroupDescriptor desc{
        .label = "perView",
        .layout = cachedPipeline.bindGroupLayouts[PipelineBuilder::getViewBindGroupIndex()],
        .entryCount = uint32_t(viewBindGroupEntries.size()),
        .entries = viewBindGroupEntries.data(),
    };

    cachedPipeline.viewBindGroup.reset(wgpuDeviceCreateBindGroup(context.wgpuDevice, &desc));
    assert(cachedPipeline.viewBindGroup);
  }

  void setupBuffers(const PipelineDrawables &pipelineDrawables, const ViewData &viewData) {
    CachedPipeline &cachedPipeline = *pipelineDrawables.cachedPipeline.get();

    // Update view buffers
    size_t index{};
    for (auto &binding : cachedPipeline.viewBuffersBindings) {
      fillViewBuffer(cachedPipeline.viewBuffers[index].handle, binding.layout, cachedPipeline, viewData);
      index++;
    }

    // Update draw instance buffers
    index = 0;
    for (auto &binding : cachedPipeline.drawBufferBindings) {
      auto &instanceDesc = cachedPipeline.drawBufferInstanceDescriptions[index];

      instanceDesc.bufferStride = binding.layout.getArrayStride();
      instanceDesc.bufferLength = instanceDesc.bufferStride * pipelineDrawables.drawablesSorted.size();
      auto &buffer = cachedPipeline.instanceBufferPool.allocateBuffer(instanceDesc.bufferLength);
      fillInstanceBuffer(buffer, binding.layout, pipelineDrawables, viewData.view);

      auto &entry = cachedPipeline.drawBindGroupEntries[binding.index];
      entry.buffer = buffer;
      entry.binding = binding.index;
      entry.size = instanceDesc.bufferLength;

      index++;
    }
  }

  void buildPipeline(CachedPipeline &cachedPipeline) {
    PipelineBuilder builder(cachedPipeline);
    cachedPipeline.pipeline.reset(builder.build(context.wgpuDevice, deviceLimits.limits));
    assert(cachedPipeline.pipeline);

    setupFixedSizeBuffers(cachedPipeline);
    buildBaseDrawParameters(cachedPipeline);
  }

  WGPUBindGroup createDrawBindGroup(const PipelineDrawables &pipelineDrawables, size_t staringIndex,
                                    const TextureIds &textureIds) {
    CachedPipeline &cachedPipeline = *pipelineDrawables.cachedPipeline.get();

    // Shift all draw buffer bindings based on starting index
    for (auto &binding : cachedPipeline.drawBufferBindings) {
      auto &instanceDesc = cachedPipeline.drawBufferInstanceDescriptions[binding.index];
      auto &entry = cachedPipeline.drawBindGroupEntries[binding.index];

      size_t objectBufferOffset = staringIndex * instanceDesc.bufferStride;
      size_t objectBufferRemainingSize = instanceDesc.bufferLength - objectBufferOffset;
      entry.offset = objectBufferOffset;
      entry.size = objectBufferRemainingSize;
    }

    // Update texture bindings
    size_t index{};
    for (auto &id : textureIds.textures) {
      auto &binding = cachedPipeline.textureBindingLayout.bindings[index++];

      auto &textureEntry = cachedPipeline.drawBindGroupEntries[binding.binding];
      textureEntry.binding = binding.binding;

      auto &samplerEntry = cachedPipeline.drawBindGroupEntries[binding.defaultSamplerBinding];
      samplerEntry.binding = binding.defaultSamplerBinding;

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

    WGPUBindGroupDescriptor desc{
        .label = "perDraw",
        .layout = cachedPipeline.bindGroupLayouts[PipelineBuilder::getDrawBindGroupIndex()],
        .entryCount = uint32_t(cachedPipeline.drawBindGroupEntries.size()),
        .entries = cachedPipeline.drawBindGroupEntries.data(),
    };

    WGPUBindGroup result = wgpuDeviceCreateBindGroup(context.wgpuDevice, &desc);
    assert(result);

    return result;
  }

  void renderPipelineDrawables(WGPURenderPassEncoder passEncoder, const PipelineDrawables &pipelineDrawables,
                               const ViewData &viewData) {
    CachedPipeline &cachedPipeline = *pipelineDrawables.cachedPipeline.get();
    if (pipelineDrawables.drawablesSorted.empty())
      return;

    setupBuffers(pipelineDrawables, viewData);

    wgpuRenderPassEncoderSetPipeline(passEncoder, cachedPipeline.pipeline);

    // Set view bindgroup
    wgpuRenderPassEncoderSetBindGroup(passEncoder, PipelineBuilder::getViewBindGroupIndex(), cachedPipeline.viewBindGroup, 0,
                                      nullptr);

    for (auto &drawGroup : pipelineDrawables.drawGroups) {
      if (drawGroup.key.clipRect) {
        auto &clipRect = drawGroup.key.clipRect.value();
        wgpuRenderPassEncoderSetScissorRect(passEncoder, clipRect.x, clipRect.y, clipRect.z - clipRect.x,
                                            clipRect.w - clipRect.y);
      } else {
        wgpuRenderPassEncoderSetScissorRect(passEncoder, viewData.viewport.x, viewData.viewport.y, viewData.viewport.width,
                                            viewData.viewport.height);
      }

      WGPUBindGroup drawBindGroup = createDrawBindGroup(pipelineDrawables, drawGroup.startIndex, drawGroup.key.textures);
      wgpuRenderPassEncoderSetBindGroup(passEncoder, PipelineBuilder::getDrawBindGroupIndex(), drawBindGroup, 0, nullptr);
      onFrameCleanup([drawBindGroup]() { wgpuBindGroupRelease(drawBindGroup); });

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

    renderGraphEvaluator.reset(frameCounter);

    auto mainOutputResolution = mainOutput.texture->getResolution();
    viewStack.push(ViewStack::Item{
        .viewport = Rect(mainOutputResolution),
        .referenceSize = mainOutputResolution,
    });
  }

  void endFrame() {
    viewStack.pop(); // Main output
    viewStack.reset();

    ensureMainOutputCleared();

    clearOldCacheItems();
  }

  // Checks values in a map for the lastTouched member
  // if not used in `frameThreshold` frames, remove it
  template <typename T> void clearOldCacheItemsIn(T &iterable, size_t frameThreshold) {
    for (auto it = iterable.begin(); it != iterable.end();) {
      auto &value = it->second;
      if ((frameCounter - value->lastTouched) > frameThreshold) {
        it = iterable.erase(it);
      } else {
        ++it;
      }
    }
  }

  void clearOldCacheItems() {
    clearOldCacheItemsIn(pipelineCache.map, 16);
    clearOldCacheItemsIn(viewCache, 4);
    clearOldCacheItemsIn(drawableCache, 4);
  }

  void ensureMainOutputCleared() {
    if (!renderGraphEvaluator.isWrittenTo(mainOutput.texture)) {
      RenderGraphBuilder builder;
      builder.nodes.resize(1);
      builder.allocateOutputs(0, steps::makeRenderStepOutput(RenderStepOutput::Texture{
                                     .handle = mainOutput.texture,
                                 }));
      auto graph = builder.finalize();

      renderGraphEvaluator.evaluate(graph, std::vector<TexturePtr>{}, context, frameCounter);
    }
  }

  void swapBuffers() {
    ++frameCounter;
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

  void fillInstanceBuffer(DynamicWGPUBuffer &instanceBuffer, const shader::UniformBufferLayout &layout,
                          const PipelineDrawables &pipelineDrawables, View &view) {
    auto &cachedPipeline = *pipelineDrawables.cachedPipeline.get();
    size_t numObjects = pipelineDrawables.drawablesSorted.size();
    size_t stride = layout.getArrayStride();

    size_t instanceBufferLength = numObjects * stride;
    instanceBuffer.resize(context.wgpuDevice, instanceBufferLength, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst,
                          "instances");

    std::vector<uint8_t> drawDataTempBuffer;
    drawDataTempBuffer.resize(instanceBufferLength);
    for (size_t i = 0; i < numObjects; i++) {
      const SortableDrawable &sortableDrawable = pipelineDrawables.drawablesSorted[i];
      const Drawable *drawable = sortableDrawable.drawable;

      DrawData objectDrawData = cachedPipeline.baseDrawData;
      objectDrawData.setParam("world", drawable->transform);

      float4x4 worldInverse = linalg::inverse(drawable->transform);
      objectDrawData.setParam("invWorld", worldInverse);
      objectDrawData.setParam("invTransWorld", linalg::transpose(worldInverse));

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
      FeatureCallbackContext callbackContext{context, &view, drawable, sortableDrawable.cachedDrawable};
      for (const Feature *feature : cachedPipeline.features) {
        for (const FeatureDrawDataFunction &drawDataGenerator : feature->drawData) {
          // TODO: Catch mismatch errors here
          drawDataGenerator(callbackContext, objectDrawData);
        }
      }

      size_t bufferOffset = stride * i;
      size_t remainingBufferLength = instanceBufferLength - bufferOffset;

      packDrawData(drawDataTempBuffer.data() + bufferOffset, remainingBufferLength, layout, objectDrawData);
    }

    wgpuQueueWriteBuffer(context.wgpuQueue, instanceBuffer, 0, drawDataTempBuffer.data(), drawDataTempBuffer.size());
  }

  void buildBaseDrawParameters(CachedPipeline &cachedPipeline) {
    for (const Feature *feature : cachedPipeline.features) {
      for (auto &param : feature->shaderParams) {
        cachedPipeline.baseDrawData.setParam(param.name, param.defaultValue);
      }
    }
  }

  void depthSortBackToFront(PipelineDrawables &pipelineDrawables, const View &view) {
    const CachedView &cachedView = *viewCache[&view].get();

    size_t numDrawables = pipelineDrawables.drawablesSorted.size();
    for (size_t i = 0; i < numDrawables; i++) {
      SortableDrawable &sortable = pipelineDrawables.drawablesSorted[i];
      float4 projected = mul(cachedView.viewProjectionTransform, mul(sortable.drawable->transform, float4(0, 0, 0, 1)));
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

  SortableDrawable createSortableDrawable(PipelineDrawables &pipelineDrawables, const Drawable &drawable,
                                          const CachedDrawable &cachedDrawable) {
    SortableDrawable sortableDrawable{};
    sortableDrawable.drawable = &drawable;
    sortableDrawable.cachedDrawable = &cachedDrawable;
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

      CachedDrawable &cachedDrawable = getCachedDrawable(drawable);
      cachedDrawable.touchWithNewTransform(drawable->transform, frameCounter);
      if (drawable->previousTransform) {
        cachedDrawable.previousTransform = drawable->previousTransform.value();
      }

      SortableDrawable sortableDrawable = createSortableDrawable(pipelineDrawables, *drawable, cachedDrawable);

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

  void fillViewBuffer(WGPUBuffer &buffer, const shader::UniformBufferLayout &layout, CachedPipeline &cachedPipeline,
                      const ViewData &viewData) {
    std::vector<uint8_t> drawDataTempBuffer;
    drawDataTempBuffer.resize(layout.size);

    DrawData drawData;
    setViewData(drawData, viewData);

    // Grab draw data from features
    FeatureCallbackContext callbackContext{.context = context, .view = &viewData.view, .cachedView = &viewData.cachedView};
    for (const Feature *feature : cachedPipeline.features) {
      for (const FeatureDrawDataFunction &drawDataGenerator : feature->viewData) {
        drawDataGenerator(callbackContext, drawData);
      }
    }

    packDrawData(drawDataTempBuffer.data(), layout.size, layout, drawData);

    wgpuQueueWriteBuffer(context.wgpuQueue, buffer, 0, drawDataTempBuffer.data(), drawDataTempBuffer.size());
  }

  void collectReferencedRenderAttachments(const PipelineDrawables &pipelineDrawables, std::vector<Texture *> &outTextures) {
    // Find references to render textures
    pipelineDrawables.textureIdMap.forEachTexture([&](Texture *texture) {
      if (textureFormatFlagsContains(texture->getFormat().flags, TextureFormatFlags::RenderAttachment)) {
        outTextures.push_back(texture);
      }
    });
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

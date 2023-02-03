#include "renderer.hpp"
#include "context.hpp"
#include "drawable.hpp"
#include "feature.hpp"
#include "fwd.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "mesh_utils.hpp"
#include "params.hpp"
#include "pipeline_builder.hpp"
#include "renderer_types.hpp"
#include "renderer_cache.hpp"
#include "texture_cache.hpp"
#include "render_graph.hpp"
#include "drawables/mesh_drawable.hpp"
#include "shader/blocks.hpp"
#include "shader/entry_point.hpp"
#include "shader/fmt.hpp"
#include "shader/generator.hpp"
#include "shader/textures.hpp"
#include "shader/uniforms.hpp"
#include "steps/defaults.hpp"
#include "drawable_processor.hpp"
#include "texture.hpp"
#include "texture_placeholder.hpp"
#include "view.hpp"
#include "render_target.hpp"
#include "log.hpp"
#include "geom.hpp"
#include "worker_memory.hpp"
#include "pmr/vector.hpp"
#include "pmr/unordered_map.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/reduce.hpp>
#include <tracy/Tracy.hpp>
#include <thread>
#include <algorithm>
#include <tracy/Tracy.hpp>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>
#include <span>
#include <type_traits>

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

struct PipelineGroup {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;

  shards::pmr::vector<const IDrawable *> drawables;
  shards::pmr::vector<Feature *> baseFeatures;
  CachedPipelinePtr pipeline;

  GeneratorData *generatorData{};

  // Data generator from drawable processor
  TransientPtr prepareData;

  PipelineGroup() = default;
  PipelineGroup(allocator_type allocator) : drawables(allocator), baseFeatures(allocator) {}
  PipelineGroup(PipelineGroup &&other) = default;
  PipelineGroup(PipelineGroup &&other, allocator_type allocator) : drawables(allocator), baseFeatures(allocator) {
    *this = std::move(other);
  }
  PipelineGroup &operator=(PipelineGroup &&) = default;

  void resetPostRender() { prepareData.destroy(); }
};

struct WorkerData {
  PipelineHashCache hashCache;
  std::unordered_map<Hash128, PipelineGroup> pipelineGroups;

  // Reset worker data before rendering
  void resetPreRender() { hashCache.reset(); }
};

struct FrameStats {
  size_t numDrawables{};

  void reset() { numDrawables = 0; }
};

// Temporary view for edge cases (no output written)
struct TempView {
  ViewPtr view = std::make_shared<View>();
  CachedView cached;
  Rect viewport;
  operator ViewData() {
    return ViewData{
        .view = view,
        .cachedView = cached,
        .viewport = viewport,
    };
  }
};

// The render graph and outputs
struct PreparedRenderView {
  const RenderGraph &renderGraph;
  ViewData viewData;
  std::span<TextureSubResource> renderGraphOutputs;
};

struct RendererImpl final : public ContextData {
  Renderer &outer;
  Context &context;

  WGPUSupportedLimits deviceLimits = {};

  ViewStack viewStack;

  Renderer::MainOutput mainOutput;
  bool shouldUpdateMainOutputFromContext = false;

  std::unordered_map<const View *, CachedViewDataPtr> viewCache;
  RenderGraphCache renderGraphCache;
  PipelineCache pipelineCache;
  RenderTextureCache renderTextureCache;
  TextureViewCache textureViewCache;

  bool ignoreCompilationErrors = false;

  const size_t maxBufferedFrames = GFX_RENDERER_MAX_BUFFERED_FRAMES;

  // Within the range [0, maxBufferedFrames)
  size_t frameIndex = 0;

  // Increments forever
  size_t frameCounter = 0;

  FrameStats frameStats;

  WorkerMemory workerMemory;
  WorkerData workerData;

  std::shared_mutex drawableProcessorsMutex;
  std::map<DrawableProcessorConstructor, std::shared_ptr<IDrawableProcessor>> drawableProcessors;

  bool mainOutputWrittenTo{};

  std::list<TransientPtr> processorDynamicValueCleanupQueue;

  RendererImpl(Context &context, Renderer &outer) : outer(outer), context(context), workerMemory(), workerData() {}

  ~RendererImpl() { releaseContextDataConditional(); }

  WorkerMemory &getWorkerMemoryForCurrentFrame() { return workerMemory; }

  void initializeContextData() {
    gfxWgpuDeviceGetLimits(context.wgpuDevice, &deviceLimits);
    bindToContext(context);
  }

  virtual void releaseContextData() override {
    context.poll();

    // Flush in-flight frame resources
    for (size_t i = 0; i < maxBufferedFrames; i++) {
      swapBuffers();
    }

    renderGraphCache.clear();
    pipelineCache.clear();
    viewCache.clear();
  }

  void processProcessorDynamicValueCleanupQueue() {
    auto it = processorDynamicValueCleanupQueue.begin();
    while (it != processorDynamicValueCleanupQueue.end()) {
      it->destroy();
      it = processorDynamicValueCleanupQueue.erase(it);
    }
  }

  size_t alignToMinUniformOffset(size_t size) const { return alignTo(size, deviceLimits.limits.minUniformBufferOffsetAlignment); }
  size_t alignToArrayBounds(size_t size, size_t elementAlign) const { return alignTo(size, elementAlign); }

  void updateMainOutputFromContext() {
    ZoneScoped;

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

  void renderView(ViewPtr view, const PipelineSteps &pipelineSteps) {
    ZoneScoped;

    auto prepared = prepareRenderView(view, pipelineSteps);
    if (!prepared)
      return;

    RenderGraphEvaluator evaluator(getWorkerMemoryForCurrentFrame(), renderTextureCache, textureViewCache);
    evaluator.evaluate(prepared->renderGraph, prepared->viewData, prepared->renderGraphOutputs, context, frameCounter);

    mainOutputWrittenTo = mainOutputWrittenTo || evaluator.isWrittenTo(mainOutput.texture);
  }

  std::optional<PreparedRenderView> prepareRenderView(ViewPtr view, const PipelineSteps &pipelineSteps) {
    ViewData viewData{
        .view = view,
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
      return std::nullopt; // Skip rendering

    viewData.cachedView.touchWithNewTransform(view->view, view->getProjectionMatrix(float2(viewSize)), frameCounter);

    // Match with attached outputs in buildRenderGraph
    shards::pmr::vector<TextureSubResource> renderGraphOutputs(getWorkerMemoryForCurrentFrame());
    if (viewData.renderTarget) {
      for (auto &attachment : viewData.renderTarget->attachments) {
        renderGraphOutputs.push_back(attachment.second);
      }
    } else {
      renderGraphOutputs.push_back(mainOutput.texture);
    }

    return PreparedRenderView{
        .renderGraph = getOrBuildRenderGraph(viewData, pipelineSteps, viewStackOutput.referenceSize),
        .viewData = viewData,
        .renderGraphOutputs = std::span(renderGraphOutputs.data(), renderGraphOutputs.size()),
    };
  }

  const RenderGraph &getOrBuildRenderGraph(const ViewData &viewData, const PipelineSteps &pipelineSteps,
                                           int2 referenceOutputSize) {
    ZoneScoped;

    HasherXXH128<PipelineHashVisitor> hasher;
    for (auto &step : pipelineSteps) {
      std::visit([&](auto &step) { hasher(step.id); }, *step.get());
    }

    hasher(referenceOutputSize);
    if (viewData.renderTarget) {
      for (auto &attachment : viewData.renderTarget->attachments) {
        hasher(attachment.first);
        hasher(attachment.second.texture->getFormat().pixelFormat);
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

  void buildRenderGraph(const ViewData &viewData, const PipelineSteps &pipelineSteps, int2 referenceOutputSize,
                        CachedRenderGraph &out) {
    ZoneScoped;

    RenderGraphBuilder builder;

    builder.setReferenceOutputSize(referenceOutputSize);

    size_t index{};
    for (auto &step : pipelineSteps) {
      std::visit([&](auto &&arg) { allocateNodeEdges(builder, index++, arg); }, *step.get());
    }

    // Attach outputs
    // NOTE: This doesn't actually attach the textures
    //   it just references them for type information
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

  Hash128 generateSharedHash(RenderTargetLayout &rtl) {
    HasherXXH128<PipelineHashVisitor> sharedHasher;
    sharedHasher(rtl);
    return sharedHasher.getDigest();
  }

  void allocateNodeEdges(detail::RenderGraphBuilder &builder, size_t index, const RenderDrawablesStep &step) {
    builder.allocateOutputs(index, step.output ? step.output.value() : defaultRenderStepOutput);
  }

  void setupRenderGraphNode(CachedRenderGraph &out, size_t index, const ViewData &viewData, const RenderDrawablesStep &step) {
    auto &node = out.getNode(index);

    if (!step.drawQueue)
      throw std::runtime_error("No draw queue assigned to drawable step");

    node.encode = [=, this](RenderGraphEncodeContext &ctx) { evaluateDrawableStep(ctx, step, viewData); };
  }

  void evaluateDrawableStep(RenderGraphEncodeContext &evaluateContext, const RenderDrawablesStep &step,
                            const ViewData &viewData) {
    shards::pmr::vector<Feature *> evaluateFeatures(getWorkerMemoryForCurrentFrame());
    for (auto &feature : step.features)
      evaluateFeatures.push_back(feature.get());

    renderDrawables(evaluateContext, step.drawQueue, evaluateFeatures, step.sortMode);
  }

  struct FeatureDrawableGeneratorContextImpl final : FeatureDrawableGeneratorContext {
    PipelineGroup &group;
    shards::pmr::vector<ParameterStorage> *drawParameters{};

    FeatureDrawableGeneratorContextImpl(PipelineGroup &group, Renderer &renderer, ViewPtr view,
                                        const detail::CachedView &cachedView, DrawQueuePtr queue)
        : FeatureDrawableGeneratorContext(renderer, view, cachedView, queue), group(group) {}

    virtual size_t getSize() const override { return group.drawables.size(); }

    virtual IParameterCollector &getParameterCollector(size_t index) override {
      assert(index <= drawParameters->size());
      return (*drawParameters)[index];
    }

    virtual const IDrawable &getDrawable(size_t index) override { return *group.drawables[index]; }

    virtual void render(std::vector<ViewPtr> views, const PipelineSteps &pipelineSteps) override {
      renderer.render(views, pipelineSteps);
    }
  };

  struct FeatureViewGenerateContextImpl final : public FeatureViewGeneratorContext {
    using FeatureViewGeneratorContext::FeatureViewGeneratorContext;
    ParameterStorage *viewParameters{};

    virtual IParameterCollector &getParameterCollector() override { return *viewParameters; }

    virtual void render(std::vector<ViewPtr> views, const PipelineSteps &pipelineSteps) override {
      renderer.render(views, pipelineSteps);
    }
  };

  // Runs all feature generators and returns the resulting generated parameters
  GeneratorData *runGenerators(PipelineGroup &group, const ViewData &viewData, const DrawQueuePtr &queue) {
    WorkerMemory &memory = getWorkerMemoryForCurrentFrame();

    GeneratorData *generatorData = memory->new_object<GeneratorData>();

    if (!group.pipeline->perViewGenerators.empty()) {
      generatorData->viewParameters = memory->new_object<ParameterStorage>();

      FeatureViewGenerateContextImpl viewCtx(outer, viewData.view, viewData.cachedView, queue);
      viewCtx.viewParameters = generatorData->viewParameters;
      for (auto &generator : group.pipeline->perViewGenerators) {
        viewCtx.features = generator.otherFeatures;
        generator.callback(viewCtx);
      }
    }

    if (!group.pipeline->perObjectGenerators.empty()) {
      generatorData->drawParameters = memory->new_object<shards::pmr::vector<ParameterStorage>>();

      FeatureDrawableGeneratorContextImpl objectCtx(group, outer, viewData.view, viewData.cachedView, queue);
      objectCtx.drawParameters = generatorData->drawParameters;
      for (auto &generator : group.pipeline->perObjectGenerators) {
        objectCtx.features = generator.otherFeatures;
        generator.callback(objectCtx);
      }
    }

    return generatorData;
  }

  void renderDrawables(RenderGraphEncodeContext &evaluateContext, DrawQueuePtr queue,
                       const shards::pmr::vector<Feature *> &features, SortMode sortMode) {
    ZoneScoped;

    const std::vector<const IDrawable *> &drawables = queue->getDrawables();

    const RenderTargetLayout &rtl = evaluateContext.node.renderTargetLayout;

    WorkerMemory &workerMemory = getWorkerMemoryForCurrentFrame();

    tf::Taskflow flow;

    HasherXXH128<PipelineHashVisitor> sharedHasher;
    sharedHasher(rtl);
    Hash128 stepSharedHash = sharedHasher.getDigest();

    workerData.resetPreRender();

    shards::pmr::vector<const IDrawable *> expandedDrawables(workerMemory);

    auto &pipelineGroups = *workerMemory->new_object<shards::pmr::unordered_map<Hash128, PipelineGroup>>();

    // Compute drawable hashes
    {
      ZoneScopedN("pipelineGrouping");

      {
        // Expand drawables
        ZoneScopedN("expandDrawables");
        for (auto &drawable : drawables) {
          if (!drawable->expand(expandedDrawables))
            expandedDrawables.push_back(drawable);
        }
      }

      // Now hash them
      for (const IDrawable *drawable : expandedDrawables) {
        ZoneScopedN("hashDrawable");

        PipelineHashCollector collector{.storage = &workerData.hashCache};
        collector(size_t(drawable->getProcessor())); // Hash processor to be sure it's tied to the pipeline grouping
        collector(stepSharedHash);
        collector.hashObject(*drawable);
        for (auto stepFeature : features)
          collector.hashObject(*stepFeature);
        Hash128 pipelineHash = collector.getDigest();

        auto &entry = getCacheEntry(pipelineGroups, pipelineHash, [&](const Hash128 &hash) {
          // Setup new PipelineGroup with features from step that apply
          PipelineGroup newGroup(workerMemory);
          for (auto stepFeature : features) {
            newGroup.baseFeatures.push_back(stepFeature);
          }
          return newGroup;
        });
        entry.drawables.push_back(drawable);
      }
    }

    // Retrieve existing pipelines from cache
    {
      ZoneScopedN("retrievePipelinesFromCache");

      for (auto &group : pipelineGroups) {
        auto it = pipelineCache.map.find(group.first);
        if (it != pipelineCache.map.end()) {
          group.second.pipeline = it->second;
        }
      }
    }

    auto buildPipeline = [&](PipelineGroup &group) {
      ZoneScopedN("buildPipeline");

      const IDrawable &firstDrawable = *group.drawables[0];

      // Construct processor instance, or get existing one
      drawableProcessorsMutex.lock();
      auto constructor = firstDrawable.getProcessor();
      auto it = drawableProcessors.find(constructor);
      if (it == drawableProcessors.end()) {
        it = drawableProcessors.emplace(constructor, constructor(context)).first;
      }
      drawableProcessorsMutex.unlock();
      auto &processor = *it->second.get();

      group.pipeline = std::make_shared<CachedPipeline>();
      group.pipeline->drawableProcessor = &processor;

      PipelineBuilder builder(*group.pipeline.get(), rtl, firstDrawable);

      // Add base features
      for (auto &baseFeature : group.baseFeatures) {
        builder.features.push_back(baseFeature);
      }

      // Assume same processor
      processor.buildPipeline(builder);
      builder.build(context.wgpuDevice, deviceLimits.limits);

      if (!ignoreCompilationErrors && group.pipeline->compilationError.has_value()) {
        throw formatException("Failed to build pipeline: {}", group.pipeline->compilationError->message);
      }
    };

    auto preparePipelineGroup = [&](PipelineGroup &group) {
      ZoneScopedN("preparePipelineGroup");

      auto &cachedPipeline = *group.pipeline.get();

      DrawablePrepareContext prepareContext{
          .context = context,
          .workerMemory = workerMemory,
          .cachedPipeline = cachedPipeline,
          .viewData = evaluateContext.viewData,
          .drawables = group.drawables,
          .generatorData = *group.generatorData,
          .frameCounter = frameCounter,
          .sortMode = sortMode,
      };

      auto prepareData = group.pipeline->drawableProcessor->prepare(prepareContext);
      group.prepareData = prepareData;

      // Defer cleanup
      processorDynamicValueCleanupQueue.emplace_back(prepareData);

      // Record stats
      frameStats.numDrawables += group.drawables.size();
    };

    for (auto &it : pipelineGroups) {
      PipelineGroup &group = it.second;
      // Early out
      if (group.drawables.empty())
        continue;

      // Build the pipeline
      if (!group.pipeline) {
        buildPipeline(group);
      }

      if (group.pipeline->compilationError.has_value())
        continue;

      // Run generators
      group.generatorData = runGenerators(group, evaluateContext.viewData, queue);

      // Build draw data
      preparePipelineGroup(group);
    }

    // Finally, encode render commands
    {
      ZoneScopedN("encodeRenderCommands");

      auto &viewport = evaluateContext.viewData.viewport;
      wgpuRenderPassEncoderSetViewport(evaluateContext.encoder, (float)viewport.x, (float)viewport.y, (float)viewport.width,
                                       (float)viewport.height, 0.0f, 1.0f);

      for (auto &it : pipelineGroups) {
        PipelineGroup &group = it.second;
        if (group.pipeline->compilationError.has_value())
          continue;

        DrawableEncodeContext encodeCtx{
            .encoder = evaluateContext.encoder,
            .cachedPipeline = *group.pipeline.get(),
            .viewData = evaluateContext.viewData,
            .preparedData = group.prepareData,
        };
        encodeCtx.cachedPipeline.drawableProcessor->encode(encodeCtx);
      }
    }

    // Insert generated pipelines into cache and touch used pipeline frame counters
    {
      ZoneScopedN("updateCache");

      for (auto &group : pipelineGroups) {
        auto it = pipelineCache.map.find(group.first);
        if (it == pipelineCache.map.end()) {
          pipelineCache.map.insert(std::make_pair(group.first, group.second.pipeline));
        }
        // Mark as used this frame
        touchCacheItemPtr(group.second.pipeline, frameCounter);
      }
    }
  }

  void allocateNodeEdges(detail::RenderGraphBuilder &builder, size_t index, const RenderFullscreenStep &step) {
    static auto defaultFullscreenOutput = RenderStepOutput{
        .attachments =
            {
                steps::getDefaultColorOutput(),
            },
    };

    builder.allocateInputs(index, step.inputs);

    bool overwriteTargets = step.overlay ? false : true;
    builder.allocateOutputs(index, step.output ? step.output.value() : defaultFullscreenOutput, overwriteTargets);
  }

  void setupRenderGraphNode(CachedRenderGraph &out, size_t index, const ViewData &viewData, const RenderFullscreenStep &step) {
    ZoneScoped;

    RenderGraphNode &node = out.getNode(index);

    struct StepData {
      MeshDrawable::Ptr drawable;
      FeaturePtr baseFeature;
      DrawQueuePtr queue;
      shards::pmr::vector<Feature *> features;
    };

    std::shared_ptr<StepData> data = std::make_shared<StepData>();

    MeshDrawable::Ptr &drawable = data->drawable = std::make_shared<MeshDrawable>(fullscreenQuad);
    FeaturePtr &baseFeature = data->baseFeature = std::make_shared<Feature>();

    data->queue = std::make_shared<DrawQueue>();
    data->queue->add(drawable);

    data->features.push_back(baseFeature.get());
    for (auto &feature : step.features) {
      data->features.push_back(feature.get());
    }

    // Set parameters from step
    drawable->parameters = step.parameters;

    // Setup node outputs as texture slots
    for (auto &frameIndex : node.readsFrom) {
      auto &frame = out.renderGraph.frames[frameIndex];
      baseFeature->textureParams.emplace_back(frame.name);
    }

    node.encode = [this, data, viewData](RenderGraphEncodeContext &ctx) {
      // Connect texture inputs
      for (auto &frameIndex : ctx.node.readsFrom) {
        auto &texture = ctx.evaluator.getTexture(frameIndex);
        auto &frame = ctx.graph.frames[frameIndex];
        data->drawable->parameters.set(frame.name, texture);
      }

      renderDrawables(ctx, data->queue, data->features, SortMode::Queue);
    };
  }

  void beginFrame() {
    // This registers ContextData so that releaseContextData is called when GPU resources are invalidated
    if (!isBoundToContext())
      initializeContextData();

    swapBuffers();

    if (shouldUpdateMainOutputFromContext) {
      updateMainOutputFromContext();
    }

    frameStats.reset();

    auto mainOutputResolution = mainOutput.texture->getResolution();
    viewStack.push(ViewStack::Item{
        .viewport = Rect(mainOutputResolution),
        .referenceSize = mainOutputResolution,
    });

    for (auto &drawableProcessor : drawableProcessors) {
      drawableProcessor.second->reset(frameCounter);
    }

    resetWorkerMemory();
  }

  void resetWorkerMemory() { workerMemory.reset(); }

  void endFrame() {
    viewStack.pop(); // Main output
    viewStack.reset();

    ensureMainOutputCleared();

    clearOldCacheItems();

    processProcessorDynamicValueCleanupQueue();

    TracyPlot("Drawables Processed", int64_t(frameStats.numDrawables));
  }

  void clearOldCacheItems() {
    clearOldCacheItemsIn(pipelineCache.map, frameCounter, 120 * 60 * 5);
    clearOldCacheItemsIn(viewCache, frameCounter, 120 * 60 * 5);

    // NOTE: Because textures take up a lot of memory, try to clean this as frequent as possible
    renderTextureCache.resetAndClearOldCacheItems(frameCounter, 8);
    textureViewCache.clearOldCacheItems(frameCounter, 8);
  }

  void ensureMainOutputCleared() {
    if (!mainOutputWrittenTo) {
      RenderGraphBuilder builder;
      builder.nodes.resize(1);
      builder.allocateOutputs(0, steps::makeRenderStepOutput(RenderStepOutput::Texture{
                                     .subResource = mainOutput.texture,
                                 }));
      auto graph = builder.finalize();

      RenderGraphEvaluator evaluator(getWorkerMemoryForCurrentFrame(), renderTextureCache, textureViewCache);

      TempView tempView{.viewport = Rect(mainOutput.texture->getResolution())};
      evaluator.evaluate(graph, tempView, std::span<TextureSubResource>{}, context, frameCounter);
    }
  }

  void swapBuffers() {
    ++frameCounter;
    frameIndex = (frameIndex + 1) % maxBufferedFrames;
  }
};

Renderer::Renderer(Context &context) {
  impl = std::make_shared<RendererImpl>(context, *this);
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

void Renderer::setIgnoreCompilationErrors(bool ignore) { impl->ignoreCompilationErrors = ignore; }

void Renderer::dumpStats() {
  static constexpr size_t Megabyte = 1 << 20;

  SPDLOG_LOGGER_INFO(logger, "== Renderer stats ==");

  size_t workerIndex{};
  SPDLOG_LOGGER_INFO(logger, " Graphics worker allocators:");
  auto &mem = impl->workerMemory;
  SPDLOG_LOGGER_INFO(logger, "  Worker[{}]: {} MB (peak: {} MB, avg: {} MB)", workerIndex++,
                     mem.getMemoryResource().preallocatedBlock.size() / Megabyte,
                     mem.getMemoryResource().maxUsage.getMax() / Megabyte,
                     mem.getMemoryResource().maxUsage.getAverage() / Megabyte);

  SPDLOG_LOGGER_INFO(logger, " Caches:");
  SPDLOG_LOGGER_INFO(logger, "  Render Graphs: {}", impl->renderGraphCache.map.size());
  SPDLOG_LOGGER_INFO(logger, "  Pipelines: {}", impl->pipelineCache.map.size());
  SPDLOG_LOGGER_INFO(logger, "  Views: {}", impl->viewCache.size());

  SPDLOG_LOGGER_INFO(logger, " Frame Index: {}", impl->frameIndex);
  SPDLOG_LOGGER_INFO(logger, " Frame Counter: {}", impl->frameCounter);
}

} // namespace gfx

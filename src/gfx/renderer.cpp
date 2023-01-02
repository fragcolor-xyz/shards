#include "renderer.hpp"
#include "context.hpp"
#include "drawable.hpp"
#include "feature.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "mesh_utils.hpp"
#include "params.hpp"
#include "pipeline_builder.hpp"
#include "renderer_types.hpp"
#include "renderer_cache.hpp"
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
#include "async.hpp"
#include "worker_memory.hpp"
#include "pmr/vector.hpp"
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/reduce.hpp>
#include <tracy/Tracy.hpp>
#include <thread>
#include <algorithm>
#include <tracy/Tracy.hpp>
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

struct PipelineGroup {
  std::vector<const IDrawable *> drawables;
  std::vector<Feature *> baseFeatures;
  CachedPipelinePtr pipeline;

  // Data generator from drawable processor
  TransientPtr prepareData;

  void resetPreRender() { drawables.clear(); }
  void resetPostRender() { prepareData.destroy(); }
};

struct WorkerData {
  PipelineHashCache hashCache;
  std::unordered_map<Hash128, PipelineGroup> pipelineGroups;

  // Reset worker data before rendering
  void resetPreRender() {
    hashCache.reset();
    for (auto &it : pipelineGroups)
      it.second.resetPreRender();
  }
};

struct FrameStats {
  size_t numDrawables{};

  void reset() { numDrawables = 0; }
};

struct RendererImpl final : public ContextData {
  Context &context;
  WGPUSupportedLimits deviceLimits = {};

  ViewStack viewStack;

  Renderer::MainOutput mainOutput;
  bool shouldUpdateMainOutputFromContext = false;

  std::unordered_map<const View *, CachedViewDataPtr> viewCache;
  RenderGraphCache renderGraphCache;
  PipelineCache pipelineCache;

  RenderGraphEvaluator renderGraphEvaluator;

  const size_t maxBufferedFrames = GFX_RENDERER_MAX_BUFFERED_FRAMES;

  // Within the range [0, maxBufferedFrames)
  size_t frameIndex = 0;

  // Increments forever
  size_t frameCounter = 0;

  FrameStats frameStats;

  GraphicsExecutor &executor;
  TWorkerThreadData<WorkerMemory> workerMemory;
  TWorkerThreadData<WorkerData> workerData;

  std::shared_mutex drawableProcessorsMutex;
  std::map<DrawableProcessorConstructor, std::shared_ptr<IDrawableProcessor>> drawableProcessors;

  // Cache variables
  std::vector<TexturePtr> renderGraphOutputsTemp;
  std::unordered_map<Hash128, PipelineGroup> pipelineGroupsTemp;
  std::list<TransientPtr> processorDynamicValueCleanupQueue;
  std::vector<Feature *> evaluateFeaturesTemp;

  RendererImpl(Context &context)
      : context(context), executor(context.getGraphicsExecutor()), workerMemory(executor), workerData(executor) {}

  ~RendererImpl() { releaseContextDataConditional(); }

  WorkerMemories &getWorkerMemoriesForCurrentFrame() { return workerMemory; }

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
    auto &renderGraphOutputs = renderGraphOutputsTemp;
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
    ZoneScoped;

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
    ZoneScoped;

    HasherXXH128<PipelineHashVisitor> hasher;
    for (auto &step : pipelineSteps) {
      std::visit([&](auto &step) { hasher(step.id); }, *step.get());
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

    node.encode = [=](RenderGraphEncodeContext &ctx) { evaluateDrawableStep(ctx, step, viewData); };
  }

  void evaluateDrawableStep(RenderGraphEncodeContext &evaluateContext, const RenderDrawablesStep &step,
                            const ViewData &viewData) {
    DrawQueue &queue = *step.drawQueue.get();
    evaluateFeaturesTemp.clear();
    for (auto &feature : step.features)
      evaluateFeaturesTemp.push_back(feature.get());
    renderDrawables(evaluateContext, queue.getDrawables(), evaluateFeaturesTemp, step.sortMode, viewData);
  }

  void renderDrawables(RenderGraphEncodeContext &evaluateContext, const std::vector<const IDrawable *> &drawables,
                       const std::vector<Feature *> &features, SortMode sortMode, const ViewData &viewData) {
    ZoneScoped;

    const RenderTargetLayout &rtl = evaluateContext.node.renderTargetLayout;

    WorkerMemories &workerMemories = getWorkerMemoriesForCurrentFrame();

    tf::Taskflow flow;

    HasherXXH128<PipelineHashVisitor> sharedHasher;
    sharedHasher(rtl);
    Hash128 stepSharedHash = sharedHasher.getDigest();

    for (auto &workerData : this->workerData) {
      workerData.resetPreRender();
    }

    std::optional<shards::pmr::vector<const IDrawable *>> expandedDrawablesOpt;

    // Compute drawable hashes
    auto groupTask = flow.emplace([&](tf::Subflow &sub) {
      ZoneScopedN("pipelineGrouping");

      expandedDrawablesOpt.emplace(workerMemories.get());
      auto &expandedDrawables = expandedDrawablesOpt.value();

      {
        // Expand drawables
        ZoneScopedN("expandDrawables");
        for (auto &drawable : drawables) {
          if (!drawable->expand(expandedDrawables))
            expandedDrawables.push_back(drawable);
        }
      }

      // Now hash them
      sub.for_each(expandedDrawables.begin(), expandedDrawables.end(), [&](const IDrawable *drawable) {
        ZoneScopedN("hashDrawable");
        auto &workerData = this->workerData.get();

        PipelineHashCollector collector{.storage = &workerData.hashCache};
        collector(size_t(drawable->getProcessor())); // Hash processor to be sure it's tied to the pipeline grouping
        collector(stepSharedHash);
        collector.hashObject(*drawable);
        Hash128 pipelineHash = collector.getDigest();

        for (auto stepFeature : features)
          collector.hashObject(*stepFeature);

        auto &entry = getCacheEntry(workerData.pipelineGroups, pipelineHash, [&](const Hash128 &hash) {
          // Setup new PipelineGroup with features from step that apply
          PipelineGroup newGroup;
          for (auto stepFeature : features) {
            newGroup.baseFeatures.push_back(stepFeature);
          }
          return newGroup;
        });
        entry.drawables.push_back(drawable);
      });
    });

    // Combine all groups from workers
    auto pipelineGroupsMerged = this->pipelineGroupsTemp;
    pipelineGroupsMerged.clear();
    auto reduceDrawablesTask = flow.transform_reduce(
        workerData.begin(), workerData.end(), pipelineGroupsMerged,
        [&](decltype(pipelineGroupsMerged) result, const decltype(pipelineGroupsMerged) &b) {
          ZoneScopedN("pipelineGrouping (combine)");

          for (auto &it : b) {
            auto it1 = result.find(it.first);
            if (it1 == result.end()) {
              // Insert directly
              result.insert(it);
            } else {
              // Merge into existing
              for (auto &drawable : it.second.drawables) {
                it1->second.drawables.push_back(drawable);
              }
            }
          }
          return result;
        },
        [](WorkerData &worker) { return std::move(worker.pipelineGroups); });
    reduceDrawablesTask.succeed(groupTask);

    // Retrieve existing pipelines from cache
    auto retrievePipelinesFromCacheTask = flow.emplace([&]() {
      ZoneScopedN("retrievePipelinesFromCache");

      for (auto &group : pipelineGroupsMerged) {
        auto it = pipelineCache.map.find(group.first);
        if (it != pipelineCache.map.end()) {
          group.second.pipeline = it->second;
        }
      }
    });
    retrievePipelinesFromCacheTask.succeed(reduceDrawablesTask);

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
    };

    auto preparePipelineGroup = [&](tf::Subflow &subflow, PipelineGroup &group) {
      ZoneScopedN("preparePipelineGroup");

      auto &cachedPipeline = *group.pipeline.get();

      DrawablePrepareContext prepareContext{
          .context = context,
          .workerMemory = workerMemories,
          .cachedPipeline = cachedPipeline,
          .subflow = subflow,
          .viewData = viewData,
          .drawables = group.drawables,
          .sortMode = sortMode,
      };
      auto prepareData = group.pipeline->drawableProcessor->prepare(prepareContext);
      group.prepareData = prepareData;

      // Defer cleanup
      processorDynamicValueCleanupQueue.emplace_back(prepareData);

      // Record stats
      frameStats.numDrawables += group.drawables.size();
    };

    // Build new pipelines
    auto buildPipelinesAndDrawData = [&](tf::Subflow &subflow) {
      for (auto &it : pipelineGroupsMerged) {
        PipelineGroup &group = it.second;
        auto buildTask = subflow.emplace([&]() {
          // Early out
          if (group.drawables.empty())
            return;

          if (!group.pipeline) {
            // Build the pipeline
            buildPipeline(group);
          }
        });

        auto prepareTask = subflow.emplace([&](tf::Subflow &subflow) {
          // Build draw data
          preparePipelineGroup(subflow, group);
        });
        prepareTask.succeed(buildTask);
      }
    };
    auto buildPipelinesAndDrawDataTask = flow.emplace(buildPipelinesAndDrawData).succeed(retrievePipelinesFromCacheTask);

    // Finally, encode render commands
    auto encodeRenderCommands = [&](tf::Subflow &subflow) {
      ZoneScopedN("encodeRenderCommands");

      auto &viewport = viewData.viewport;
      wgpuRenderPassEncoderSetViewport(evaluateContext.encoder, (float)viewport.x, (float)viewport.y, (float)viewport.width,
                                       (float)viewport.height, 0.0f, 1.0f);

      for (auto &it : pipelineGroupsMerged) {
        PipelineGroup &group = it.second;
        DrawableEncodeContext encodeCtx{
            .encoder = evaluateContext.encoder,
            .cachedPipeline = *group.pipeline.get(),
            .viewData = viewData,
            .preparedData = group.prepareData,
        };
        encodeCtx.cachedPipeline.drawableProcessor->encode(encodeCtx);
      }
    };
    auto encodeRenderCommandsTask = flow.emplace(encodeRenderCommands).succeed(buildPipelinesAndDrawDataTask);

    // Insert generated pipelines into cache and touch used pipeline frame counters
    auto updateCache = [&]() {
      ZoneScopedN("updateCache");

      for (auto &group : pipelineGroupsMerged) {
        auto it = pipelineCache.map.find(group.first);
        if (it == pipelineCache.map.end()) {
          pipelineCache.map.insert(std::make_pair(group.first, group.second.pipeline));
        }
        // Mark as used this frame
        touchCacheItemPtr(group.second.pipeline, frameCounter);
      }
    };
    auto updateCacheTask = flow.emplace(updateCache).succeed(buildPipelinesAndDrawDataTask);

    executor.run(flow).wait();
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
      std::vector<const IDrawable *> drawables;
      std::vector<Feature *> features;
    };

    std::shared_ptr<StepData> data = std::make_shared<StepData>();
    MeshDrawable::Ptr &drawable = data->drawable = std::make_shared<MeshDrawable>(fullscreenQuad);
    FeaturePtr &baseFeature = data->baseFeature = std::make_shared<Feature>();
    data->drawables.push_back(drawable.get());

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

      renderDrawables(ctx, data->drawables, data->features, SortMode::Queue, viewData);
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

    renderGraphEvaluator.reset(frameCounter);

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

  void resetWorkerMemory() {
    for (auto &workerMemory : this->workerMemory) {
      workerMemory.reset();
    }
  }

  void endFrame() {
    viewStack.pop(); // Main output
    viewStack.reset();

    ensureMainOutputCleared();

    clearOldCacheItems();

    processProcessorDynamicValueCleanupQueue();

    TracyPlot("Drawables Processed", int64_t(frameStats.numDrawables));
  }

  void clearOldCacheItems() {
    clearOldCacheItemsIn(pipelineCache.map, frameCounter, 16);
    clearOldCacheItemsIn(viewCache, frameCounter, 4);
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

void Renderer::dumpStats() {
  static constexpr size_t Megabyte = 1 << 20;

  SPDLOG_LOGGER_INFO(logger, "== Renderer stats ==");

  auto &workerMemory = impl->workerMemory;

  size_t workerIndex{};
  SPDLOG_LOGGER_INFO(logger, " Graphics worker allocators:");
  for (auto &mem : workerMemory) {
    SPDLOG_LOGGER_INFO(logger, "  Worker[{}]: {} MB (peak: {} MB, avg: {} MB)", workerIndex++,
                       mem.getMemoryResource().preallocatedBlock.size() / Megabyte,
                       mem.getMemoryResource().maxUsage.getMax() / Megabyte,
                       mem.getMemoryResource().maxUsage.getAverage() / Megabyte);
  }

  SPDLOG_LOGGER_INFO(logger, " Caches:");
  SPDLOG_LOGGER_INFO(logger, "  Render Graphs: {}", impl->renderGraphCache.map.size());
  SPDLOG_LOGGER_INFO(logger, "  Pipelines: {}", impl->pipelineCache.map.size());
  SPDLOG_LOGGER_INFO(logger, "  Views: {}", impl->viewCache.size());

  SPDLOG_LOGGER_INFO(logger, " Frame Index: {}", impl->frameIndex);
  SPDLOG_LOGGER_INFO(logger, " Frame Counter: {}", impl->frameCounter);
}
} // namespace gfx

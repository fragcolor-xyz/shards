#include "render_step_impl.hpp"
#include "drawables/mesh_drawable.hpp"
#include "render_graph.hpp"
#include "renderer_types.hpp"
#include "shader/log.hpp"
#include "steps/defaults.hpp"
#include "geom.hpp"
#include "mesh_utils.hpp"
#include "transient_ptr.hpp"
#include "pmr/unordered_map.hpp"
#include "pmr/vector.hpp"
#include "renderer_storage.hpp"
#include <tracy/Tracy.hpp>
#include <taskflow/taskflow.hpp>

namespace gfx::detail {

static auto defaultRenderStepOutput = steps::getDefaultRenderStepOutput();
static auto defaultFullscreenOutput = RenderStepOutput{
    .attachments =
        {
            steps::getDefaultColorOutput(),
        },
};

static MeshPtr fullscreenQuad = []() {
  geom::QuadGenerator quadGen;
  quadGen.optimizeForFullscreen = true;
  quadGen.generate();
  for (auto &v : quadGen.vertices) {
    v.position[2] = 0.5f;
  }
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

struct FeatureDrawableGeneratorContextImpl final : FeatureDrawableGeneratorContext {
  PipelineGroup &group;
  shards::pmr::vector<ParameterStorage> *drawParameters{};

  FeatureDrawableGeneratorContextImpl(PipelineGroup &group, Renderer &renderer, ViewPtr view,
                                      const detail::CachedView &cachedView, DrawQueuePtr queue, size_t frameCounter)
      : FeatureDrawableGeneratorContext(renderer, view, cachedView, queue, frameCounter), group(group) {}

  virtual size_t getSize() const override { return group.drawables.size(); }

  virtual IParameterCollector &getParameterCollector(size_t index) override {
    assert(index <= drawParameters->size());
    return (*drawParameters)[index];
  }

  virtual const IDrawable &getDrawable(size_t index) override { return *group.drawables[index]; }

  virtual void render(ViewPtr view, const PipelineSteps &pipelineSteps) override { renderer.render(view, pipelineSteps, true); }
};

struct FeatureViewGenerateContextImpl final : public FeatureViewGeneratorContext {
  using FeatureViewGeneratorContext::FeatureViewGeneratorContext;
  ParameterStorage *viewParameters{};

  virtual IParameterCollector &getParameterCollector() override { return *viewParameters; }

  virtual void render(ViewPtr view, const PipelineSteps &pipelineSteps) override { renderer.render(view, pipelineSteps, true); }
};

// Runs all feature generators and returns the resulting generated parameters
GeneratorData *runGenerators(RenderGraphEvaluator &eval, PipelineGroup &group, const ViewData &viewData,
                             const DrawQueuePtr &queue) {
  auto &storage = eval.getStorage();
  auto &memory = storage.workerMemory;
  GeneratorData *generatorData = memory->new_object<GeneratorData>();

  if (!group.pipeline->perViewGenerators.empty()) {
    generatorData->viewParameters = memory->new_object<ParameterStorage>();

    FeatureViewGenerateContextImpl viewCtx(eval.getRenderer(), viewData.view, viewData.cachedView, queue, storage.frameCounter);
    viewCtx.viewParameters = generatorData->viewParameters;
    for (auto &generator : group.pipeline->perViewGenerators) {
      viewCtx.features = generator.otherFeatures;
      generator.callback(viewCtx);
    }
  }

  if (!group.pipeline->perObjectGenerators.empty()) {
    generatorData->drawParameters = memory->new_object<shards::pmr::vector<ParameterStorage>>(group.drawables.size());

    FeatureDrawableGeneratorContextImpl objectCtx(group, eval.getRenderer(), viewData.view, viewData.cachedView, queue,
                                                  storage.frameCounter);
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
  RenderGraphEvaluator &evaluator = evaluateContext.evaluator;
  RendererStorage &storage = evaluator.getStorage();
  WorkerMemory &workerMemory = storage.workerMemory;

  tf::Taskflow flow;

  HasherXXH128<PipelineHashVisitor> sharedHasher;
  sharedHasher(rtl);
  sharedHasher(evaluateContext.viewData.cachedView.isFlipped);
  Hash128 stepSharedHash = sharedHasher.getDigest();

  shards::pmr::vector<const IDrawable *> expandedDrawables(workerMemory);

  auto &pipelineGroups = *workerMemory->new_object<shards::pmr::unordered_map<Hash128, PipelineGroup>>();
  auto &hashCache = *workerMemory->new_object<PipelineHashCache>();
  auto &context = evaluator.getRenderer().getContext();

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

      PipelineHashCollector collector{.storage = &hashCache};
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
      auto it = storage.pipelineCache.map.find(group.first);
      if (it != storage.pipelineCache.map.end()) {
        group.second.pipeline = it->second;
      }
    }
  }

  auto buildPipeline = [&](PipelineGroup &group) {
    ZoneScopedN("buildPipeline");

    const IDrawable &firstDrawable = *group.drawables[0];

    // Construct processor instance, or get existing one
    auto constructor = firstDrawable.getProcessor();
    auto &processor = storage.drawableProcessorCache.get(constructor);

    group.pipeline = std::make_shared<CachedPipeline>();
    group.pipeline->drawableProcessor = &processor;

    PipelineBuilder builder(*group.pipeline.get(), rtl, firstDrawable);

    builder.isRenderingFlipped = evaluateContext.viewData.cachedView.isFlipped;

    // Add base features
    for (auto &baseFeature : group.baseFeatures) {
      builder.features.push_back(baseFeature);
    }

    // Assume same processor
    try {
      processor.buildPipeline(builder);
      builder.build(context.wgpuDevice, storage.deviceLimits.limits);
    } catch (std::exception &ex) {
      if (storage.ignoreCompilationErrors) {
        std::string msg = fmt::format("Ignored shader compilation error: {}", ex.what());
        SPDLOG_LOGGER_ERROR(gfx::shader::getLogger(), "{}", msg);
        group.pipeline->compilationError.emplace(msg);
      } else {
        throw;
      }
    }

    if (!storage.ignoreCompilationErrors && group.pipeline->compilationError.has_value()) {
      throw formatException("Failed to build pipeline: {}", group.pipeline->compilationError->message);
    }
  };

  auto preparePipelineGroup = [&](PipelineGroup &group) {
    ZoneScopedN("preparePipelineGroup");

    auto &cachedPipeline = *group.pipeline.get();

    DrawablePrepareContext prepareContext{
        .context = context,
        .storage = storage,
        .cachedPipeline = cachedPipeline,
        .viewData = evaluateContext.viewData,
        .drawables = group.drawables,
        .generatorData = *group.generatorData,
        .sortMode = sortMode,
    };

    auto prepareData = group.pipeline->drawableProcessor->prepare(prepareContext);
    group.prepareData = prepareData;

    // Defer cleanup
    storage.transientPtrCleanupQueue.emplace_back(prepareData);

    // Record stats
    storage.frameStats.numDrawables += group.drawables.size();
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
    group.generatorData = runGenerators(evaluator, group, evaluateContext.viewData, queue);

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
      auto it = storage.pipelineCache.map.find(group.first);
      if (it == storage.pipelineCache.map.end()) {
        storage.pipelineCache.map.insert(std::make_pair(group.first, group.second.pipeline));
      }
      // Mark as used this frame
      touchCacheItemPtr(group.second.pipeline, storage.frameCounter);
    }
  }
}

void evaluateDrawableStep(RenderGraphEncodeContext &evaluateContext, const RenderDrawablesStep &step) {
  auto &storage = evaluateContext.evaluator.getStorage();
  shards::pmr::vector<Feature *> evaluateFeatures(storage.workerMemory);
  for (auto &feature : step.features)
    evaluateFeatures.push_back(feature.get());

  renderDrawables(evaluateContext, step.drawQueue, evaluateFeatures, step.sortMode);
}

using Data = RenderGraphBuilder::NodeBuildData;

void allocateNodeEdges(detail::RenderGraphBuilder &builder, Data &data, const RenderFullscreenStep &step) {
  builder.allocateInputs(data, step.inputs);

  bool overwriteTargets = step.overlay ? false : true;
  builder.allocateOutputs(data, step.output ? step.output.value() : defaultFullscreenOutput, overwriteTargets);
}

static inline TextureSampleType getDefaultTextureSampleType(WGPUTextureFormat pixelFormat) {
  TextureSampleType compatibleSampleTypes = getTextureFormatDescription(pixelFormat).compatibleSampleTypes;
  if (uint8_t(compatibleSampleTypes) & uint8_t(TextureSampleType::Float)) {
    return TextureSampleType::Float;
  } else if (uint8_t(compatibleSampleTypes) & uint8_t(TextureSampleType::UInt)) {
    return TextureSampleType::UInt;
  } else if (uint8_t(compatibleSampleTypes) & uint8_t(TextureSampleType::Int)) {
    return TextureSampleType::Int;
  } else if (uint8_t(compatibleSampleTypes) & uint8_t(TextureSampleType::UnfilterableFloat)) {
    return TextureSampleType::UnfilterableFloat;
  } else {
    throw std::logic_error("No default compatible sample type found for texture format");
  }
}

void setupRenderGraphNode(RenderGraphNode &node, RenderGraphBuilder::NodeBuildData &buildData, const RenderFullscreenStep &step) {
  ZoneScoped;

  struct StepData {
    MeshDrawable::Ptr drawable;
    FeaturePtr baseFeature;
    DrawQueuePtr queue;
    shards::pmr::vector<FeaturePtr> capturedFeatures;
    shards::pmr::vector<Feature *> features;
  };

  std::shared_ptr<StepData> data = std::make_shared<StepData>();

  MeshDrawable::Ptr &drawable = data->drawable = std::make_shared<MeshDrawable>(fullscreenQuad);
  FeaturePtr &baseFeature = data->baseFeature = std::make_shared<Feature>();

  data->queue = std::make_shared<DrawQueue>();
  data->queue->add(drawable);

  data->features.push_back(baseFeature.get());
  for (auto &feature : step.features) {
    data->capturedFeatures.push_back(feature);
    data->features.push_back(feature.get());
  }

  // Derive definitions from parameters
  for (auto &param : step.parameters.basic) {
    baseFeature->shaderParams.emplace_back(param.first, param.second);
  }
  for (auto &param : step.parameters.textures) {
    auto &textureFormat = param.second.texture->getFormat();
    auto &entry = baseFeature->textureParams.emplace_back(param.first, textureFormat.dimension);
    entry.type.format = getDefaultTextureSampleType(textureFormat.pixelFormat);
  }

  // Setup node outputs as texture slots
  for (auto &frame : buildData.readsFrom) {
    auto &entry = baseFeature->textureParams.emplace_back(frame->name);
     entry.type.format = getDefaultTextureSampleType(frame->format);
  }

  node.encode = [data, &step, drawable](RenderGraphEncodeContext &ctx) {
    // Set parameters from step
    drawable->parameters = step.parameters;

    // Connect texture inputs
    for (auto &frameIndex : ctx.node.readsFrom) {
      auto &texture = ctx.evaluator.getTexture(frameIndex);
      auto &frame = ctx.graph.frames[frameIndex];
      data->drawable->parameters.set(frame.name, texture);
    }

    renderDrawables(ctx, data->queue, data->features, SortMode::Queue);
  };
}

void allocateNodeEdges(detail::RenderGraphBuilder &builder, Data &data, const ClearStep &step) {
  builder.allocateOutputs(data, step.output ? step.output.value() : defaultRenderStepOutput, true);
}

void setupRenderGraphNode(RenderGraphNode &node, RenderGraphBuilder::NodeBuildData &buildData, const ClearStep &step) {
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

void allocateNodeEdges(detail::RenderGraphBuilder &builder, Data &data, const RenderDrawablesStep &step) {
  builder.allocateOutputs(data, step.output ? step.output.value() : defaultRenderStepOutput);
}

void setupRenderGraphNode(RenderGraphNode &node, RenderGraphBuilder::NodeBuildData &buildData, const RenderDrawablesStep &step) {
  if (!step.drawQueue)
    throw std::runtime_error("No draw queue assigned to drawable step");

  if (step.drawQueue->isAutoClear())
    buildData.autoClearQueues.push_back(step.drawQueue);

  node.encode = [=](RenderGraphEncodeContext &ctx) { evaluateDrawableStep(ctx, step); };
}
} // namespace gfx::detail

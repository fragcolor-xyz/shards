#include "renderer.hpp"
#include "boost/container/stable_vector.hpp"
#include "context.hpp"
#include "drawable.hpp"
#include "feature.hpp"
#include "fwd.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "mesh_utils.hpp"
#include "params.hpp"
#include "pipeline_builder.hpp"
#include "renderer_storage.hpp"
#include "renderer_types.hpp"
#include "renderer_cache.hpp"
#include "texture_view_cache.hpp"
#include "render_graph.hpp"
#include "drawables/mesh_drawable.hpp"
#include "shader/blocks.hpp"
#include "shader/entry_point.hpp"
#include "shader/fmt.hpp"
#include "shader/generator.hpp"
#include "shader/textures.hpp"
#include "shader/uniforms.hpp"
#include "drawable_processor.hpp"
#include "texture.hpp"
#include "texture_placeholder.hpp"
#include "view.hpp"
#include "render_target.hpp"
#include "log.hpp"
#include "geom.hpp"
#include "view_stack.hpp"
#include "worker_memory.hpp"
#include "pmr/vector.hpp"
#include "pmr/unordered_map.hpp"
#include "frame_queue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/reduce.hpp>
#include <tracy/Tracy.hpp>
#include <thread>
#include <algorithm>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>
#include <span>
#include <type_traits>

#define GFX_RENDERER_MAX_BUFFERED_FRAMES (2)

using namespace gfx::detail;

namespace gfx {

static auto logger = gfx::getLogger();

// The render graph and outputs
struct PreparedRenderView {
  const RenderGraph &renderGraph;
  ViewData viewData;
  std::span<TextureSubResource> renderGraphOutputs;
};

struct RendererImpl final : public ContextData {
  Renderer &outer;
  Context &context;

  RendererStorage storage;

  ViewStack viewStack;

  Renderer::MainOutput mainOutput;
  RenderTargetPtr mainOutputRenderTarget;
  bool shouldUpdateMainOutputFromContext = false;

  bool ignoreCompilationErrors = false;

  const size_t maxBufferedFrames = GFX_RENDERER_MAX_BUFFERED_FRAMES;

  bool mainOutputWrittenTo{};

  boost::container::stable_vector<std::optional<FrameQueue>> frameQueues;

  RendererImpl(Context &context, Renderer &outer) : outer(outer), context(context), storage(context) {}

  ~RendererImpl() { releaseContextDataConditional(); }

  WorkerMemory &getWorkerMemoryForCurrentFrame() { return storage.workerMemory; }

  void initializeContextData() {
    assert(context.isReady());
    gfxWgpuDeviceGetLimits(context.wgpuDevice, &storage.deviceLimits);
    bindToContext(context);
  }

  virtual void releaseContextData() override {
    context.poll();

    // Flush in-flight frame resources
    for (size_t i = 0; i < maxBufferedFrames; i++) {
      swapBuffers();
    }

    storage.renderGraphCache.clear();
    storage.pipelineCache.clear();
    storage.viewCache.clear();
  }

  void processTransientPtrCleanupQueue() {
    auto it = storage.transientPtrCleanupQueue.begin();
    while (it != storage.transientPtrCleanupQueue.end()) {
      it->destroy();
      it = storage.transientPtrCleanupQueue.erase(it);
    }
  }

  void updateMainOutputFromContext() {
    ZoneScoped;

    if (!mainOutput.texture) {
      mainOutput.texture = std::make_shared<Texture>();
    }

    WGPUTextureView view = context.getMainOutputTextureView();
    int2 resolution = context.getMainOutputSize();

    auto currentDesc = mainOutput.texture->getDesc();
    bool needUpdate = !mainOutputRenderTarget || currentDesc.externalTexture != view || currentDesc.resolution != resolution;
    if (needUpdate) {
      mainOutput.texture
          ->init(TextureDesc{
              .format =
                  TextureFormat{
                      .dimension = TextureDimension::D2,
                      .flags = TextureFormatFlags::RenderAttachment | TextureFormatFlags::NoTextureBinding,
                      .pixelFormat = context.getMainOutputFormat(),
                  },
              .resolution = resolution,
              .externalTexture = view,
          })
          .initWithLabel("mainOutput");
    }
  }

  CachedView &getCachedView(const ViewPtr &view) { return *getSharedCacheEntry<CachedView>(storage.viewCache, view->getId()).get(); }

  void pushView(ViewStack::Item &&item) {
    viewStack.push(std::move(item));
    const auto &viewStackOutput = viewStack.getOutput();

    bool needsNewFrameQueue = false;
    if (viewStack.size() == 1)
      needsNewFrameQueue = true;
    else {
      auto prevItem = viewStack.getOutput(1);
      if (prevItem.renderTarget != viewStackOutput.renderTarget) {
        needsNewFrameQueue = true;
      }
    }

    if (needsNewFrameQueue) {
      viewStackOutput.renderTarget->resizeConditional(viewStackOutput.referenceSize);
      auto &frameQueue = frameQueues.emplace_back();
      frameQueue.emplace(viewStackOutput.renderTarget, storage, getWorkerMemoryForCurrentFrame());
      frameQueue->fallbackClearColor.emplace(float4(1, 1, 1, 1));
    } else {
      // Can render to the exisiting queue
      frameQueues.push_back(std::nullopt);
    }
  }

  void popView() {
    auto &frameQueue = frameQueues.back();

    // Evaluate the queue for the popped view
    if (frameQueue.has_value()) {
      frameQueue->evaluate(outer);
    }

    frameQueues.pop_back();
    viewStack.pop();
  }

  FrameQueue &getFrameQueue() {
    for (auto it = frameQueues.rbegin(); it != frameQueues.rend(); ++it) {
      if (it->has_value())
        return it->value();
    }

    // Somehow there's no FrameQueue to enque render commands into
    throw std::logic_error("No frame queue in view stack");
  }

  void render(ViewPtr view, const PipelineSteps &pipelineSteps, bool immediate) {
    ZoneScoped;

    ViewData viewData{
        .view = view,
        .cachedView = getCachedView(view),
    };

    ViewStack::Output viewStackOutput = viewStack.getOutput();
    viewData.viewport = viewStackOutput.viewport;
    viewData.renderTarget = viewStackOutput.renderTarget;
    viewData.referenceOutputSize = viewStackOutput.referenceSize;

    int2 viewSize = viewStackOutput.viewport.getSize();
    if (viewSize.x == 0 || viewSize.y == 0)
      return;

    viewData.cachedView.touchWithNewTransform(view->view, view->getProjectionMatrix(float2(viewSize)), storage.frameCounter);

    getFrameQueue().enqueue(viewData, pipelineSteps);
  }

  void beginFrame() {
    // This registers ContextData so that releaseContextData is called when GPU resources are invalidated
    if (!isBoundToContext())
      initializeContextData();

    swapBuffers();

    if (shouldUpdateMainOutputFromContext) {
      updateMainOutputFromContext();
    }

    // Update main render target
    if (!mainOutputRenderTarget)
      mainOutputRenderTarget = std::make_shared<RenderTarget>("mainOutput");
    mainOutputRenderTarget->attachments["color"].texture = mainOutput.texture;
    mainOutputRenderTarget->resizeFixed(mainOutput.texture->getResolution());

    storage.frameStats.reset();

    storage.drawableProcessorCache.beginFrame(storage.frameCounter);

    resetWorkerMemory();

    auto mainOutputResolution = mainOutput.texture->getResolution();
    pushView(ViewStack::Item{
        .viewport = Rect(mainOutputResolution),
        .referenceSize = mainOutputResolution,
        .renderTarget = mainOutputRenderTarget,
    });
  }

  void resetWorkerMemory() { storage.workerMemory.reset(); }

  void endFrame() {
    // Reset view stack and pop main output
    // This also evaluates the main frame queue
    popView();
    viewStack.reset();

    clearOldCacheItems();

    processTransientPtrCleanupQueue();

#ifdef TRACY_ENABLE
    TracyPlot("Drawables Processed", int64_t(storage.frameStats.numDrawables));

    TracyPlotConfig("GFX WorkerMemory", tracy::PlotFormatType::Memory, true, true, 0);
    TracyPlot("GFX WorkerMemory", int64_t(storage.workerMemory.getMemoryResource().totalRequestedBytes));

#endif
  }

  void clearOldCacheItems() {
    clearOldCacheItemsIn(storage.pipelineCache.map, storage.frameCounter, 120 * 60 * 5);
    clearOldCacheItemsIn(storage.viewCache, storage.frameCounter, 120 * 60 * 5);

    // NOTE: Because textures take up a lot of memory, try to clean this as frequent as possible
    storage.renderTextureCache.resetAndClearOldCacheItems(storage.frameCounter, 8);
    storage.textureViewCache.clearOldCacheItems(storage.frameCounter, 8);
  }

  void swapBuffers() {
    ++storage.frameCounter;
    storage.frameIndex = (storage.frameIndex + 1) % maxBufferedFrames;
  }
};

Renderer::Renderer(Context &context) {
  impl = std::make_shared<RendererImpl>(context, *this);
  if (!context.isHeadless()) {
    impl->shouldUpdateMainOutputFromContext = true;
  }

  if (context.isReady())
    impl->initializeContextData();
}

Context &Renderer::getContext() { return impl->context; }

void Renderer::render(ViewPtr view, const PipelineSteps &pipelineSteps, bool immediate) {
  impl->render(view, pipelineSteps, immediate);
}
void Renderer::setMainOutput(const MainOutput &output) {
  impl->mainOutput = output;
  impl->shouldUpdateMainOutputFromContext = false;
}

const ViewStack &Renderer::getViewStack() const { return impl->viewStack; }

/// <div rustbindgen hide></div>
void Renderer::pushView(ViewStack::Item &&item) { impl->pushView(std::move(item)); }

/// <div rustbindgen hide></div>
void Renderer::popView() { impl->popView(); }

void Renderer::beginFrame() { impl->beginFrame(); }
void Renderer::endFrame() { impl->endFrame(); }

void Renderer::cleanup() { impl->releaseContextDataConditional(); }

void Renderer::setIgnoreCompilationErrors(bool ignore) { impl->storage.ignoreCompilationErrors = ignore; }

void Renderer::dumpStats() {
  static constexpr size_t Megabyte = 1 << 20;

  SPDLOG_LOGGER_INFO(logger, "== Renderer stats ==");

  size_t workerIndex{};
  SPDLOG_LOGGER_INFO(logger, " Graphics worker allocators:");
  auto &mem = impl->storage.workerMemory;
  SPDLOG_LOGGER_INFO(logger, "  Worker[{}]: {} MB (peak: {} MB, avg: {} MB)", workerIndex++,
                     mem.getMemoryResource().preallocatedBlock.size() / Megabyte,
                     mem.getMemoryResource().maxUsage.getMax() / Megabyte,
                     mem.getMemoryResource().maxUsage.getAverage() / Megabyte);

  SPDLOG_LOGGER_INFO(logger, " Caches:");
  SPDLOG_LOGGER_INFO(logger, "  Render Graphs: {}", impl->storage.renderGraphCache.map.size());
  SPDLOG_LOGGER_INFO(logger, "  Pipelines: {}", impl->storage.pipelineCache.map.size());
  SPDLOG_LOGGER_INFO(logger, "  Views: {}", impl->storage.viewCache.size());

  SPDLOG_LOGGER_INFO(logger, " Frame Index: {}", impl->storage.frameIndex);
  SPDLOG_LOGGER_INFO(logger, " Frame Counter: {}", impl->storage.frameCounter);
}

} // namespace gfx

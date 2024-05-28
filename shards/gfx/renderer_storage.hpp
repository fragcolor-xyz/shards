#ifndef A136DB6F_2BE4_4AC2_B49D_57A7943F990B
#define A136DB6F_2BE4_4AC2_B49D_57A7943F990B

#include "fwd.hpp"
#include "worker_memory.hpp"
#include "render_graph.hpp"
#include "texture_view_cache.hpp"
#include "drawable_processor.hpp"
#include "gizmos/shapes.hpp"

namespace gfx::detail {

struct FrameStats {
  size_t numDrawables{};

  void reset() { numDrawables = 0; }
};

using DebugVisualizer = shards::FunctionBase<512, void(gfx::ShapeRenderer &sr)>;

// Storage container for all renderer data
struct RendererStorage {
  WorkerMemory workerMemory;
  WGPUSupportedLimits deviceLimits = {};
  DrawableProcessorCache drawableProcessorCache;
  std::vector<TransientPtr> transientPtrCleanupQueue;
  RenderGraphCache renderGraphCache;
  std::unordered_map<UniqueId, CachedViewDataPtr> viewCache;
  std::unordered_map<UniqueId, QueueDataPtr> queueCache;
  PipelineCache pipelineCache;
  RenderTextureCache renderTextureCache;
  TextureViewCache textureViewCache;

  // Enable debug visualization
  bool debug{};
  Swappable<std::vector<DebugVisualizer>, 2> debugVisualizers;

  FrameStats frameStats;

  bool ignoreCompilationErrors{};

  // Within the range [0, maxBufferedFrames)
  size_t frameIndex = 0;

  // Increments forever
  size_t frameCounter = 0;

  RendererStorage(Context &context) : drawableProcessorCache(context) {}

  template <typename F> void debugVisualize(F &&f) {
    if (!debug)
      return;
    debugVisualizers.get(frameCounter % 2).emplace_back(std::forward<F>(f));
  }

  void clear() {
    renderGraphCache.clear();
    pipelineCache.clear();
    renderTextureCache.clear();
    textureViewCache.reset();
    viewCache.clear();
    queueCache.clear();
    debugVisualizers.forAll([](auto& v) { v.clear(); });
    drawableProcessorCache.drawableProcessorsMutex.lock();
    drawableProcessorCache.drawableProcessors.clear();
    drawableProcessorCache.drawableProcessorsMutex.unlock();
  }

  WGPUTextureView getTextureView(const TextureContextData &textureData, uint8_t faceIndex, uint8_t mipIndex) {
    shassert((textureData.texture || textureData.externalTexture) && "Invalid texture");
    TextureViewDesc desc{
        .format = textureData.format.pixelFormat,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = mipIndex,
        .mipLevelCount = 1,
        .baseArrayLayer = uint32_t(faceIndex),
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    };
    WGPUTextureView result = textureViewCache.getTextureView(frameCounter, textureData, desc);
    shassert(result);
    return result;
  }
};

} // namespace gfx::detail

#endif /* A136DB6F_2BE4_4AC2_B49D_57A7943F990B */

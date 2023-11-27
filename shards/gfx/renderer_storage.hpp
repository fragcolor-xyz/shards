#ifndef A136DB6F_2BE4_4AC2_B49D_57A7943F990B
#define A136DB6F_2BE4_4AC2_B49D_57A7943F990B

#include "fwd.hpp"
#include "worker_memory.hpp"
#include "render_graph.hpp"
#include "debug/debugger.hpp"
#include "texture_view_cache.hpp"
#include "drawable_processor.hpp"

namespace gfx::detail {

struct FrameStats {
  size_t numDrawables{};

  void reset() { numDrawables = 0; }
};

// Storage container for all renderer data
struct RendererStorage {
  WorkerMemory workerMemory;
  WGPUSupportedLimits deviceLimits = {};
  DrawableProcessorCache drawableProcessorCache;
  std::list<TransientPtr> transientPtrCleanupQueue;
  RenderGraphCache renderGraphCache;
  std::unordered_map<UniqueId, CachedViewDataPtr> viewCache;
  PipelineCache pipelineCache;
  RenderTextureCache renderTextureCache;
  TextureViewCache textureViewCache;
  std::shared_ptr<debug::Debugger> debugger;

  FrameStats frameStats;

  bool ignoreCompilationErrors{};

  // Within the range [0, maxBufferedFrames)
  size_t frameIndex = 0;

  // Increments forever
  size_t frameCounter = 0;

  RendererStorage(Context &context) : drawableProcessorCache(context) {}

  WGPUTextureView getTextureView(const TextureContextData &textureData, uint8_t faceIndex, uint8_t mipIndex) {
    if (textureData.externalView)
      return textureData.externalView;

    assert(textureData.texture);
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
    assert(result);
    return result;
  }
};

} // namespace gfx::detail

#endif /* A136DB6F_2BE4_4AC2_B49D_57A7943F990B */

#ifndef CE118917_ABBB_4232_B921_4B050D75BBCC
#define CE118917_ABBB_4232_B921_4B050D75BBCC

#include "boost/container/small_vector.hpp"
#include "pipeline_step.hpp"
#include "pmr/vector.hpp"
#include "renderer.hpp"
#include "render_graph.hpp"
#include "renderer_types.hpp"
#include <hasherxxh128.hpp>

namespace gfx::detail {
// Queue for all rendering operations that happen withing a frame
// The commands added into this queue should be ordered the same every frame for efficienct
// so preferably from a single thread
struct FrameQueue {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;

  struct Entry {
    ViewData viewData;
    PipelineSteps steps;
    Entry(const ViewData &viewData, const PipelineSteps &steps) : viewData(viewData), steps(steps) {}
  };

private:
  RenderGraphBuilder builder;
  shards::pmr::vector<Entry> queue;
  Renderer::MainOutput mainOutput;
  RenderGraphCache &renderGraphCache;

public:
  FrameQueue(Renderer::MainOutput mainOutput, RenderGraphCache &renderGraphCache, allocator_type allocator)
      : queue(allocator), mainOutput(mainOutput), renderGraphCache(renderGraphCache) {}

  void enqueue(const ViewData &viewData, const PipelineSteps &steps) { queue.emplace_back(viewData, steps); }

  const RenderGraph &getOrCreateRenderGraph() {
    HasherXXH128<HasherDefaultVisitor> hasher;
    for (auto &entry : queue) {
      for (auto &step : entry.steps) {
        std::visit([&](auto &step) { hasher(step.id); }, *step.get());
      }

      auto &viewData = entry.viewData;
      hasher(viewData.cachedView.isFlipped);
      hasher(viewData.referenceOutputSize);
      if (viewData.renderTarget) {
        for (auto &attachment : viewData.renderTarget->attachments) {
          hasher(attachment.first);
          hasher(attachment.second.texture->getFormat().pixelFormat);
        }
      } else {
        hasher("color");
        hasher(mainOutput.texture->getFormat().pixelFormat);
      }
    }

    Hash128 renderGraphHash = hasher.getDigest();
    auto &crg = getCacheEntry(renderGraphCache.map, renderGraphHash, [&](const Hash128 &key) {
      auto result = std::make_shared<CachedRenderGraph>();
      buildRenderGraph(queue, *result.get());
      return result;
    });

    return crg->renderGraph;
  }

  void buildRenderGraph(shards::pmr::vector<Entry> &entries, CachedRenderGraph &out) {
    RenderGraphBuilder builder;
    size_t queueDataIndex{};
    for (auto &entry : entries) {
      for (auto &step : entry.steps) {
        builder.addNode(entry.viewData, step, queueDataIndex);
      }
      queueDataIndex++;
    }

    builder.finalizeNodeConnections();
  }
};
} // namespace gfx::detail

#endif /* CE118917_ABBB_4232_B921_4B050D75BBCC */

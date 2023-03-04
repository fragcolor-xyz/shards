#ifndef CE118917_ABBB_4232_B921_4B050D75BBCC
#define CE118917_ABBB_4232_B921_4B050D75BBCC

#include "boost/container/small_vector.hpp"
#include "enums.hpp"
#include "pipeline_step.hpp"
#include "pmr/vector.hpp"
#include "renderer.hpp"
#include "render_graph.hpp"
#include "renderer_storage.hpp"
#include "renderer_types.hpp"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include <hasherxxh128.hpp>

namespace gfx::detail {
// Queue for all rendering operations that happen withing a frame
// The commands added into this queue should be ordered the same every frame for efficienct
// so preferably from a single thread
struct FrameQueue final : public IRenderGraphEvaluationData {
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
      result->renderGraph = buildRenderGraph(queue, *result.get());
      return result;
    });

    return crg->renderGraph;
  }

  void dumpRenderGraph(RenderGraph &graph) {
    static auto logger = getLogger();

    SPDLOG_LOGGER_DEBUG(logger, "Built frame render graph:");
    SPDLOG_LOGGER_DEBUG(logger, "Frames:");
    for (auto &frame : graph.frames) {
      std::string outSuffix;
      if (frame.outputIndex.has_value())
        outSuffix += fmt::format("out: {}", frame.outputIndex.value());
      if (frame.textureOverride.texture)
        outSuffix += fmt::format("overriden", frame.outputIndex.value());
      if (!outSuffix.empty())
        outSuffix = fmt::format(", {}", outSuffix);
      SPDLOG_LOGGER_DEBUG(logger, " - {} fmt: {}, size: {}{}", frame.name, frame.format, frame.size, outSuffix);
    }
    SPDLOG_LOGGER_DEBUG(logger, "Nodes:");
    for (auto &node : graph.nodes) {
      std::string writesToStr;
      for (auto &wt : node.writesTo)
        writesToStr += fmt::format("{}, ", wt.frameIndex);
      if (!writesToStr.empty())
        writesToStr.resize(writesToStr.size() - 2);

      std::string readsFromStr;
      for (auto &wt : node.readsFrom)
        readsFromStr += fmt::format("{}, ", wt);
      if (!readsFromStr.empty())
        readsFromStr.resize(readsFromStr.size() - 2);

      SPDLOG_LOGGER_DEBUG(logger, " - qdi: {}, writes: {}, reads: {}", node.queueDataIndex, writesToStr, readsFromStr);
    }
  }

  RenderGraph buildRenderGraph(shards::pmr::vector<Entry> &entries, CachedRenderGraph &out) {
    static auto logger = getLogger();

    SPDLOG_LOGGER_DEBUG(logger, "Building frame queue render graph");

    RenderGraphBuilder builder;
    builder.referenceOutputSize = float2(mainOutput.texture->getResolution());
    builder.outputs.push_back(RenderGraphBuilder::Output{
        .name = "color",
        .format = mainOutput.texture->getFormat().pixelFormat,
    });

    size_t queueDataIndex{};
    for (auto &entry : entries) {
      for (auto &step : entry.steps) {
        builder.addNode(entry.viewData, step, queueDataIndex);
      }
      queueDataIndex++;
    }

    RenderGraph rg = builder.build();

    if (logger->level() <= spdlog::level::debug) {
      dumpRenderGraph(rg);
    }

    return rg;
  }

  // Renderer is passed for generator callbacks
  void evaluate(Renderer &renderer, RendererStorage &storage) {
    const RenderGraph &rg = getOrCreateRenderGraph();
    RenderGraphEvaluator evaluator(storage.workerMemory, renderer, storage);

    shards::pmr::vector<TextureSubResource> renderGraphOutputs(storage.workerMemory);
    renderGraphOutputs.push_back(mainOutput.texture);

    evaluator.evaluate(rg, *this, renderGraphOutputs);
  }

protected:
  const ViewData &getViewData(size_t queueIndex) { return queue[queueIndex].viewData; }
};
} // namespace gfx::detail

#endif /* CE118917_ABBB_4232_B921_4B050D75BBCC */

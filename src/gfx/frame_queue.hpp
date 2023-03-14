#ifndef CE118917_ABBB_4232_B921_4B050D75BBCC
#define CE118917_ABBB_4232_B921_4B050D75BBCC

#include "boost/container/small_vector.hpp"
#include "enums.hpp"
#include "fwd.hpp"
#include "pipeline_step.hpp"
#include "pmr/vector.hpp"
#include "renderer.hpp"
#include "render_graph.hpp"
#include "renderer_storage.hpp"
#include "renderer_types.hpp"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include <hasherxxh128.hpp>
#include <initializer_list>

namespace gfx::detail {

// Temporary view for edge cases (no output written)
struct TempView {
  ViewPtr view = std::make_shared<View>();
  CachedView cached;
  Rect viewport;
  std::optional<ViewData> viewData;

  TempView() {
    viewData.emplace(ViewData{
        .view = view,
        .cachedView = cached,
        .viewport = viewport,
    });
  }
  TempView(const TempView &) = delete;
  TempView &operator=(const TempView &) const = delete;
  operator const ViewData &() const { return viewData.value(); }
};

// Queue for all rendering operations that happen withing a frame
// The commands added into this queue should be ordered the same every frame for efficienct
// so preferably from a single thread
struct FrameQueue final : public IRenderGraphEvaluationData {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;

  struct Entry {
    ViewData viewData;
    PipelineSteps steps;
    Entry(const ViewData &viewData, const PipelineSteps &steps) : viewData(viewData), steps(steps) {}
    Entry(const ViewData &viewData, const PipelineStepPtr &step) : viewData(viewData), steps({step}) {}
  };

private:
  shards::pmr::vector<Entry> queue;
  RenderTargetPtr mainOutput;
  RendererStorage &storage;

  static inline TempView tempView;

public:
  std::optional<float4> fallbackClearColor;

public:
  FrameQueue(RenderTargetPtr mainOutput, RendererStorage &storage, allocator_type allocator)
      : queue(allocator), mainOutput(mainOutput), storage(storage) {}

  void enqueue(const ViewData &viewData, const PipelineSteps &steps) { queue.emplace_back(viewData, steps); }

  const RenderGraph &getOrCreateRenderGraph() {
    HasherXXH128<HasherDefaultVisitor> hasher;
    for (auto &entry : queue) {
      for (auto &step : entry.steps) {
        std::visit([&](auto &step) { hasher(step.getId()); }, *step.get());
      }

      auto &viewData = entry.viewData;
      hasher(viewData.cachedView.isFlipped);
      hasher(viewData.referenceOutputSize);
      for (auto &attachment : mainOutput->attachments) {
        hasher(attachment.first);
        hasher(attachment.second.texture->getFormat().pixelFormat);
      }
    }

    Hash128 renderGraphHash = hasher.getDigest();
    auto &crg = getCacheEntry(storage.renderGraphCache.map, renderGraphHash, [&](const Hash128 &key) {
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
    static auto logger = gfx::getLogger();

    SPDLOG_LOGGER_DEBUG(logger, "Building frame queue render graph");

    RenderGraphBuilder builder;
    builder.referenceOutputSize = float2(mainOutput->getSize());
    for (auto &attachment : mainOutput->attachments) {
      builder.outputs.push_back(RenderGraphBuilder::Output{
          .name = attachment.first,
          .format = attachment.second.texture->getFormat().pixelFormat,
      });
    }

    size_t queueDataIndex{};
    for (auto &entry : entries) {
      for (auto &step : entry.steps) {
        builder.addNode(entry.viewData, step, queueDataIndex);
      }
      queueDataIndex++;
    }

    // Ensure cleared outputs
    if (fallbackClearColor) {
      for (auto &output : mainOutput->attachments) {
        if (!builder.isWrittenTo(output.first)) {
          auto format = output.second.texture->getFormat().pixelFormat;
          auto &formatDesc = getTextureFormatDescription(format);
          if (hasAnyTextureFormatUsage(formatDesc.usage, TextureFormatUsage::Color)) {
            ClearStep clear;
            clear.output = RenderStepOutput{};
            clear.output->attachments.emplace_back(RenderStepOutput::Named{.name = output.first, .format = format});
            clear.clearValues = ClearValues::getColorValue(fallbackClearColor.value());

            // Add a clear node, the queue index ~0 indicates that the tempView is passed during runtime
            auto &entry = entries.emplace_back(tempView, std::make_shared<PipelineStep>(clear));
            builder.addNode(entry.viewData, entry.steps[0], size_t(~0));
          }
        }
      }
    }

    RenderGraph rg = builder.build();

    if (logger->level() <= spdlog::level::debug) {
      dumpRenderGraph(rg);
    }

    return rg;
  }

  // Renderer is passed for generator callbacks
  void evaluate(Renderer &renderer) {
    const RenderGraph &rg = getOrCreateRenderGraph();
    RenderGraphEvaluator evaluator(storage.workerMemory, renderer, storage);

    shards::pmr::vector<TextureSubResource> renderGraphOutputs(storage.workerMemory);
    for (auto &attachment : mainOutput->attachments) {
      renderGraphOutputs.push_back(attachment.second);
    }

    evaluator.evaluate(rg, *this, renderGraphOutputs);
  }

protected:
  const ViewData &getViewData(size_t queueIndex) {
    if (queueIndex == size_t(~0)) {
      // This indicates temporary view / no view required
      return tempView;
    }
    return queue[queueIndex].viewData;
  }
};
} // namespace gfx::detail

#endif /* CE118917_ABBB_4232_B921_4B050D75BBCC */

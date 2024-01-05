#ifndef A49EBCE8_A7F2_4667_A2DD_1728DB22823F
#define A49EBCE8_A7F2_4667_A2DD_1728DB22823F

#include "render_graph.hpp"
#include "pipeline_step.hpp"
#include <variant>
#include <compare>

namespace gfx::detail {
namespace graph_build_data {

struct RenderGraphBuilder;
struct FrameBuildData;
struct FrameSize {
  static inline std::strong_ordering compareScale(const std::optional<float2> &a, const std::optional<float2> &b) {
    bool noScaleA = !a || a == float2(1.0f);
    bool noScaleB = !b || b == float2(1.0f);
    if ((noScaleA && noScaleB) || a == b)
      return std::strong_ordering::equal;
    if (a < b)
      return std::strong_ordering::less;
    else
      return std::strong_ordering::greater;
  }
  struct RelativeToMain {
    std::optional<float2> scale;
    std::strong_ordering operator<=>(const RelativeToMain &other) const { return compareScale(scale, other.scale); }
    bool operator==(const RelativeToMain &other) const { return compareScale(scale, other.scale) == std::strong_ordering::equal; }
  };
  struct RelativeToFrame {
    FrameBuildData *frame;
    std::optional<float2> scale;
    std::strong_ordering operator<=>(const RelativeToFrame &other) const {
      if (frame == other.frame) {
        return compareScale(scale, other.scale);
      }
      return frame <=> other.frame;
    }
    bool operator==(const RelativeToFrame &other) const {
      if (frame != other.frame)
        return false;
      return compareScale(scale, other.scale) == std::strong_ordering::equal;
    }
  };
  struct Manual {
    std::strong_ordering operator<=>(const Manual &other) const { return std::strong_ordering::equal; }
    bool operator!=(const Manual &other) const { return false; }
    bool operator==(const Manual &other) const { return true; }
  };
};
using FrameSizing = std::variant<FrameSize::RelativeToMain, FrameSize::RelativeToFrame, FrameSize::Manual>;
inline std::strong_ordering operator<=>(const FrameSizing &a, const FrameSizing &b) {
  auto ci = a.index() <=> b.index();
  if (ci != 0)
    return ci;

  return std::visit(
      [&](auto &arg) {
        using T = std::decay_t<decltype(arg)>;
        return arg <=> std::get<T>(b);
      },
      a);
}

using TextureOverrideRef = RenderGraph::TextureOverrideRef;

struct FrameBuildData {
  FastString name;
  size_t index;
  RenderGraph::FrameBinding binding;
  WGPUTextureFormat format;
  std::optional<FrameSizing> sizing;

  // (optional) The node that spawned this frame
  NodeBuildData *creator{};
  // std::optional<AttachmentRef> binding;

  // Last node index that wrote to this frame
  size_t lastWriteIndex;
};

struct FrameBuildData;
struct NodeBuildData;

struct AttachmentRef {
  NodeBuildData *node;
  FrameBuildData *attachment;
};

// Temporary data about nodes
struct Attachment {
  RenderStepOutput::OutputVariant output;
  FrameBuildData *frame;
  std::optional<ClearValues> clearValues;
};

// Check that a size matches during graph execution
struct SizeConstraintBuildData {
  size_t a, b;
  SizeConstraintBuildData(size_t a, size_t b) : a(a), b(b) {}
  bool operator==(const SizeConstraintBuildData &other) const {
    return (a == other.a && b == other.b) || (a == other.b && b == other.a);
  }
};

struct NodeBuildData {
  // Original step that created this node
  PipelineStepPtr step;

  // This points to which data slot to use when resolving view data
  size_t queueDataIndex;
  // Index in builder input
  size_t srcIndex;

  std::optional<std::reference_wrapper<const RenderStepInput>> input;
  std::optional<std::reference_wrapper<const RenderStepOutput>> output;

  std::vector<FrameBuildData *> inputs;
  std::vector<FrameBuildData *> outputs;
  std::vector<DrawQueuePtr> autoClearQueues;

  struct CopyOperation {
    enum Order {
      Before,
      After,
    } order;
    FrameBuildData *src;
    FrameBuildData *dst;
    CopyOperation(Order order, FrameBuildData *from, FrameBuildData *to) : order(order), src(from), dst(to) {}
  };
  std::vector<CopyOperation> requiredCopies;

  struct ClearOperation {
    FrameBuildData *frame;
    bool discard{};
    ClearValues clearValues;
  };
  std::vector<ClearOperation> requiredClears;

  NodeBuildData(PipelineStepPtr step, size_t queueDataIndex) : step(step), queueDataIndex(queueDataIndex) {}
  FrameBuildData *findInputFrame(FastString name) const;
  FrameBuildData *findOutputFrame(FastString name) const;
};

struct OutputBuildData {
  FrameBuildData *frame{};
};
struct InputBuildData {
  FrameBuildData *frame{};
};

struct Output {
  FastString name;
  WGPUTextureFormat format;
  Output(FastString name, WGPUTextureFormat format = WGPUTextureFormat_RGBA8UnormSrgb) : name(name), format(format) {}
};

struct SizeBuildData {
  FrameBuildData *originalFrame;
  size_t originalSizeId;
  std::optional<float2> sizeScale;
  bool isDynamic{};

  bool operator==(const SizeBuildData &other) const {
    return (originalSizeId == other.originalSizeId) && (sizeScale == other.sizeScale);
  }
};
} // namespace graph_build_data
using RenderGraphBuilder = graph_build_data::RenderGraphBuilder;
} // namespace gfx::detail

#endif /* A49EBCE8_A7F2_4667_A2DD_1728DB22823F */

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
  struct RelativeToMain {
    std::optional<float2> scale;
    std::strong_ordering operator<=>(const RelativeToMain &other) const = default;
  };
  struct RelativeToFrame {
    FrameBuildData *frame;
    std::optional<float2> scale;
    std::strong_ordering operator<=>(const RelativeToFrame &other) const = default;
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

struct TextureOverrideRef {
  // size_t stepIndex;
  PipelineStepPtr step;
  enum Binding {
    Input,
    Output,
  } bindingType;
  size_t bindingIndex;
};

struct FrameBuildData {
  std::string name;
  // int2 size;
  // int2 inputSize;
  // std::optional<SizeIndex> sizeId;
  WGPUTextureFormat format;
  // TextureSubResource textureOverride;
  std::optional<TextureOverrideRef> textureOverride;
  std::optional<FrameSizing> sizing;
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

  struct CopyOperation {
    enum Order {
      Before,
      After,
    } order;
    FrameBuildData *from;
    FrameBuildData *to;
  };
  std::vector<CopyOperation> rquiredCopies;

  NodeBuildData(PipelineStepPtr step, size_t queueDataIndex) : step(step), queueDataIndex(queueDataIndex) {}
  FrameBuildData *findInputFrame(const std::string &name) const;
};

struct OutputBuildData {
  FrameBuildData *frame{};
};
struct InputBuildData {
  FrameBuildData *frame{};
};

struct Output {
  std::string name;
  WGPUTextureFormat format;
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

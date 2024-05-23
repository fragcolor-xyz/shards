#ifndef D251FF97_80A2_4ACD_856B_0D3A776BB7A7
#define D251FF97_80A2_4ACD_856B_0D3A776BB7A7

#include "boost/container/vector.hpp"
#include "fwd.hpp"
#include "material.hpp"
#include "gfx_wgpu.hpp"
#include "render_target.hpp"
#include "unique_id.hpp"
#include <variant>
#include <vector>
#include <memory>
#include <boost/container/small_vector.hpp>
#include <boost/tti/has_member_data.hpp>
#include <spdlog/fmt/fmt.h>

namespace gfx {

namespace detail {
namespace graph_build_data {
struct RenderGraphBuilder;
}
using RenderGraphBuilder = graph_build_data::RenderGraphBuilder;
struct PipelineStepContext;
} // namespace detail

enum class SortMode {
  // Keep queue ordering,
  Queue,
  // Optimal for batching
  Batch,
  // For transparent object rendering
  BackToFront,
};

union ClearValues {
  float4 color;
  struct {
    float depth;
    uint32_t stencil;
  };

  constexpr ClearValues() : color(0.0f){};
  constexpr ClearValues(float4 color) : color(color) {}
  constexpr ClearValues(float depth, uint32_t stencil) : depth(depth), stencil(stencil) {}

  static constexpr ClearValues getColorValue(float4 color) { return ClearValues(color); }
  static constexpr ClearValues getDepthStencilValue(float depth, uint32_t stencil = 0) { return ClearValues(depth, stencil); }

  // All zero color value
  static constexpr ClearValues getDefaultColor() { return getColorValue(float4(0.0)); }

  // Depth=1.0 and stencil=0
  static constexpr ClearValues getDefaultDepthStencil() { return getDepthStencilValue(1.0f, 0); }
};

// Describes texture/render target connections between render steps
struct RenderStepOutput {
  // Relative to main output reference size
  struct RelativeToMainSize {
    // The scale, (1.0, 1.0) means same size
    std::optional<float2> scale;
  };
  // Relative to the first input to this pass
  struct RelativeToInputSize {
    // The input name, if unset, the first input is used as reference
    std::optional<FastString> name;
    // The scale, (1.0, 1.0) means same size
    std::optional<float2> scale;
  };
  // Not sized automatically, must be sized manually
  struct ManualSize {};
  using OutputSizing = std::variant<RelativeToMainSize, RelativeToInputSize, ManualSize>;

  // A managed named render frame
  struct Named {
    FastString name;

    // The desired format
    WGPUTextureFormat format = WGPUTextureFormat_RGBA8UnormSrgb;

    // When set, clear buffer with these values (based on format)
    std::optional<ClearValues> clearValues;

    Named(FastString name, WGPUTextureFormat format = WGPUTextureFormat_RGBA8UnormSrgb,
          std::optional<ClearValues> clearValues = std::nullopt)
        : name(name), format(format), clearValues(clearValues) {}
  };

  // A preallocated texture to output to
  struct Texture {
    FastString name;

    TextureSubResource subResource;

    // When set, clear buffer with these values (based on format)
    std::optional<ClearValues> clearValues;

    Texture(FastString name, TextureSubResource subResource, std::optional<ClearValues> clearValues = std::nullopt)
        : name(name), subResource(subResource), clearValues(clearValues) {}
  };

  typedef std::variant<Named, Texture> OutputVariant;

  std::vector<OutputVariant> attachments;

  // When set, will automatically scale outputs relative to main output
  // Reused buffers loaded from previous steps are upscaled/downscaled
  // Example:
  //  (0.5, 0.5) would render at half the output resolution
  //  (2.0, 2.0) would render at double the output resolution
  OutputSizing outputSizing = RelativeToMainSize{};

  RenderStepOutput() = default;
  RenderStepOutput(const RenderStepOutput &) = default;
  RenderStepOutput(RenderStepOutput &&) = default;
  RenderStepOutput &operator=(const RenderStepOutput &) = default;
  RenderStepOutput &operator=(RenderStepOutput &&) = default;

  void push(Named name) { attachments.emplace_back(std::move(name)); }
  void push(Texture texture) { attachments.emplace_back(std::move(texture)); }
  template <typename... TArgs> static inline RenderStepOutput make(TArgs... args) {
    RenderStepOutput out;
    ((out.push(args)), ...);
    return out;
  }
};

struct RenderStepInput {
  // A managed named render frame
  struct Named {
    FastString name;
    Named(FastString name) : name(name) {}
    Named(const char *name) : name(name) {}
  };

  // A preallocated texture to input
  struct Texture {
    FastString name;
    TextureSubResource subResource;
    Texture(FastString name, TextureSubResource subResource) : name(name), subResource(subResource) {}
  };

  typedef std::variant<Named, Texture> InputVariant;

  std::vector<InputVariant> attachments;

  RenderStepInput() = default;
  RenderStepInput(const RenderStepInput &) = default;
  RenderStepInput(RenderStepInput &&) = default;
  RenderStepInput &operator=(const RenderStepInput &) = default;
  RenderStepInput &operator=(RenderStepInput &&) = default;

  void push(FastString name) { attachments.emplace_back(Named(name)); }
  void push(Texture texture) { attachments.emplace_back(std::move(texture)); }
  template <typename... TArgs> static RenderStepInput make(TArgs... args) {
    RenderStepInput input;
    ((input.push(args)), ...);
    return input;
  }
};

extern UniqueIdGenerator renderStepIdGenerator;

// Step that does nothing, but can be used to clear using an empty render pass
struct NoopStep {
  // Used to identify this feature for caching purposes
  UniqueId id = renderStepIdGenerator.getNext();

  // Name that shows up in the debugger
  FastString name;

  std::optional<RenderStepOutput> output;

  std::shared_ptr<NoopStep> clone() { return cloneSelfWithId(this, renderStepIdGenerator.getNext()); };
  UniqueId getId() const { return id; }
};

// Renders all drawables in the queue to the output region
struct RenderDrawablesStep {
  // Used to identify this feature for caching purposes
  UniqueId id = renderStepIdGenerator.getNext();

  // Name that shows up in the debugger
  FastString name;

  DrawQueuePtr drawQueue;
  SortMode sortMode = SortMode::Batch;
  std::vector<FeaturePtr> features;

  // When enabled, features on drawables are ignored
  bool ignoreDrawableFeatures{};

  std::optional<RenderStepOutput> output;

  std::shared_ptr<RenderDrawablesStep> clone() { return cloneSelfWithId(this, renderStepIdGenerator.getNext()); };
  UniqueId getId() const { return id; }
};

// Renders a single item to the entire output region, used for post processing steps
struct RenderFullscreenStep {
  // Used to identify this feature for caching purposes
  UniqueId id = renderStepIdGenerator.getNext();

  // Name that shows up in the debugger
  FastString name;

  std::vector<FeaturePtr> features;
  MaterialParameters parameters;

  RenderStepInput input;
  std::optional<RenderStepOutput> output;

  // used to indicate this pass does not cover the entire output
  // e.g. some blending pass / sub-section of the output
  bool overlay{};

  std::shared_ptr<RenderFullscreenStep> clone() { return cloneSelfWithId(this, renderStepIdGenerator.getNext()); };
  UniqueId getId() const { return id; }
};

typedef std::variant<NoopStep, RenderDrawablesStep, RenderFullscreenStep> PipelineStep;
typedef std::shared_ptr<PipelineStep> PipelineStepPtr;

#if !defined(NDEBUG)
typedef std::vector<PipelineStepPtr> PipelineSteps;
#else
typedef boost::container::small_vector<PipelineStepPtr, 8> PipelineSteps;
#endif

template <typename T> PipelineStepPtr makePipelineStep(T &&step) { return std::make_shared<PipelineStep>(std::forward<T>(step)); }

BOOST_TTI_TRAIT_HAS_MEMBER_DATA(has_render_step_name, name);
inline std::string getPipelineStepName(const PipelineStepPtr &step) {
  std::string nameBuffer;
  std::visit(
      [&](auto &arg) {
        using T = std::decay_t<decltype(arg)>;
        nameBuffer = NAMEOF_FULL_TYPE(T);
        if constexpr (has_render_step_name<T, FastString>::type::value) {
          if (!arg.name.empty()) {
            nameBuffer = fmt::format("{} ({})", arg.name.c_str(), nameBuffer);
          }
        }
      },
      *step.get());
  return nameBuffer;
}
} // namespace gfx

#endif /* D251FF97_80A2_4ACD_856B_0D3A776BB7A7 */

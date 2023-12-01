#ifndef D251FF97_80A2_4ACD_856B_0D3A776BB7A7
#define D251FF97_80A2_4ACD_856B_0D3A776BB7A7

#include "fwd.hpp"
#include "material.hpp"
#include "gfx_wgpu.hpp"
#include "render_target.hpp"
#include "unique_id.hpp"
#include <variant>
#include <vector>
#include <memory>
#include <boost/container/small_vector.hpp>

namespace gfx {

namespace detail {
struct RenderGraphBuilder;
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
  // A managed named render frame
  struct Named {
    std::string name;

    // The desired format
    WGPUTextureFormat format = WGPUTextureFormat_RGBA8UnormSrgb;

    // When set, clear buffer with these values (based on format)
    std::optional<ClearValues> clearValues;
  };

  // A preallocated texture to output to
  struct Texture {
    std::string name;

    TextureSubResource subResource;

    // When set, clear buffer with these values (based on format)
    std::optional<ClearValues> clearValues;
  };

  typedef std::variant<Named, Texture> OutputVariant;

  std::vector<OutputVariant> attachments;

  // When set, will automatically scale outputs relative to main output
  // Reused buffers loaded from previous steps are upscaled/downscaled
  // Example:
  //  (0.5, 0.5) would render at half the output resolution
  //  (2.0, 2.0) would render at double the output resolution
  std::optional<float2> sizeScale = float2(1, 1);

  RenderStepOutput() = default;
  RenderStepOutput(const RenderStepOutput &) = default;
  RenderStepOutput(RenderStepOutput &&) = default;
  RenderStepOutput &operator=(const RenderStepOutput &) = default;
  RenderStepOutput &operator=(RenderStepOutput &&) = default;
};

struct RenderStepInput {
  // A managed named render frame
  struct Named {
    std::string name;
  };

  // A preallocated texture to input
  struct Texture {
    std::string name;
    TextureSubResource subResource;
  };

  typedef std::variant<Named, Texture> InputVariant;

  std::vector<InputVariant> attachments;

  RenderStepInput() = default;
  RenderStepInput(const RenderStepInput &) = default;
  RenderStepInput(RenderStepInput &&) = default;
  RenderStepInput &operator=(const RenderStepInput &) = default;
  RenderStepInput &operator=(RenderStepInput &&) = default;
};

extern UniqueIdGenerator renderStepIdGenerator;

// Explicitly clear render targets
struct ClearStep {
  // Used to identify this feature for caching purposes
  UniqueId id = renderStepIdGenerator.getNext();

  ClearValues clearValues;

  std::optional<RenderStepOutput> output;

  std::shared_ptr<ClearStep> clone() { return cloneSelfWithId(this, renderStepIdGenerator.getNext()); };
  UniqueId getId() const { return id; }
};

// Renders all drawables in the queue to the output region
struct RenderDrawablesStep {
  // Used to identify this feature for caching purposes
  UniqueId id = renderStepIdGenerator.getNext();

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

typedef std::variant<ClearStep, RenderDrawablesStep, RenderFullscreenStep> PipelineStep;
typedef std::shared_ptr<PipelineStep> PipelineStepPtr;
typedef boost::container::small_vector<PipelineStepPtr, 8> PipelineSteps;

template <typename T> PipelineStepPtr makePipelineStep(T &&step) { return std::make_shared<PipelineStep>(std::forward<T>(step)); }
} // namespace gfx

#endif /* D251FF97_80A2_4ACD_856B_0D3A776BB7A7 */

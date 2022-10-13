#ifndef GFX_PIPELINE_STEP
#define GFX_PIPELINE_STEP

#include "fwd.hpp"
#include "material.hpp"
#include "gfx_wgpu.hpp"
#include <variant>
#include <vector>
#include <memory>

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

  static constexpr ClearValues getColorValue(float4 color) { return ClearValues{.color = color}; }
  static constexpr ClearValues getDepthStencilValue(float depth, uint32_t stencil = 0) {
    return ClearValues{.depth = depth, .stencil = stencil};
  }

  // All zero color value
  static constexpr ClearValues getDefaultColor() { return getColorValue(float4(0.0)); }

  // Depth=1.0 and stencil=0
  static constexpr ClearValues getDefaultDepthStencil() { return getDepthStencilValue(1.0f, 0); }
};

// Describes texture/render target connections between render steps
struct RenderStepIO {
  enum class Mode {
    // Load buffer from previous steps
    Load,
    // Clear buffer
    Clear,
  };

  // A managed named render frame
  struct NamedOutput {
    std::string name;

    // The desired format
    WGPUTextureFormat format = WGPUTextureFormat_RGBA8UnormSrgb;

    // When set, clear buffer with these values (based on format)
    std::optional<ClearValues> clearValues;
  };

  // A preallocator texture to output to
  struct TextureOutput {
    std::string name;

    TexturePtr handle;

    // When set, clear buffer with these values (based on format)
    std::optional<ClearValues> clearValues;
  };

  typedef std::variant<NamedOutput, TextureOutput> OutputVariant;

  std::vector<std::string> inputs;
  std::vector<OutputVariant> outputs;

  // When set, will automatically scale outputes relative to main output
  // Reused buffers loaded from previous steps are upscaled/downscaled
  // Example:
  //  (0.5, 0.5) would render at half the output resolution
  //  (2.0, 2.0) would render at double the output resolution
  std::optional<float2> outputSizeScale = float2(1, 1);
};

// Explicitly clear render targets
struct ClearStep {
  ClearValues clearValues;
  std::vector<RenderStepIO::OutputVariant> outputs;
};

// Renders all drawables in the queue to the output region
struct RenderDrawablesStep {
  DrawQueuePtr drawQueue;
  SortMode sortMode = SortMode::Batch;
  std::vector<FeaturePtr> features;
  RenderStepIO io;
};

// Renders a single item to the entire output region, used for post processing steps
struct RenderFullscreenStep {
  std::vector<FeaturePtr> features;
  MaterialParameters parameters;
  RenderStepIO io;

  // used to indicate this pass does not cover the entire output
  // e.g. some blending pass / sub-section of the output
  bool overlay{};
};

typedef std::variant<ClearStep, RenderDrawablesStep, RenderFullscreenStep> PipelineStep;
typedef std::shared_ptr<PipelineStep> PipelineStepPtr;
typedef std::vector<PipelineStepPtr> PipelineSteps;

template <typename T> PipelineStepPtr makePipelineStep(T &&step) { return std::make_shared<PipelineStep>(std::move(step)); }
} // namespace gfx

#endif // GFX_PIPELINE_STEP

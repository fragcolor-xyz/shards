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

struct ClearValues {
  float4 color = float4(0, 0, 0, 0);
  float depth = 1.0f;
  uint32_t stencil = 0;
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
    Mode mode = Mode::Load;
  };

  // A preallocator texture to output to
  struct TextureOutput {
    std::string name;
    TexturePtr handle;
    Mode mode = Mode::Load;
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
};

typedef std::variant<ClearStep, RenderDrawablesStep, RenderFullscreenStep> PipelineStep;
typedef std::shared_ptr<PipelineStep> PipelineStepPtr;
typedef std::vector<PipelineStepPtr> PipelineSteps;

template <typename T> PipelineStepPtr makePipelineStep(T &&step) { return std::make_shared<PipelineStep>(std::move(step)); }
} // namespace gfx

#endif // GFX_PIPELINE_STEP

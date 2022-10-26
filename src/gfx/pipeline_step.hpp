#ifndef GFX_PIPELINE_STEP
#define GFX_PIPELINE_STEP

#include "fwd.hpp"
#include "material.hpp"
#include <variant>
#include <vector>
#include <memory>

namespace gfx {

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

struct ClearStep {
  // (optional) Render target to render to
  RenderTargetPtr renderTarget;
  ClearValues clearValues;
};

// Renders all drawables in the queue to the output region
struct RenderDrawablesStep {
  DrawQueuePtr drawQueue;
  std::vector<FeaturePtr> features;
  SortMode sortMode = SortMode::Batch;
  bool forceDepthClear = false;
  // (optional) Render target to render to
  RenderTargetPtr renderTarget;
};

// Renders a single item to the entire output region, used for post processing steps
struct RenderFullscreenStep {
  std::vector<FeaturePtr> features;
  // (optional) Render target to render to
  RenderTargetPtr renderTarget;
  MaterialParameters parameters;
};

typedef std::variant<ClearStep, RenderDrawablesStep, RenderFullscreenStep> PipelineStep;
typedef std::shared_ptr<PipelineStep> PipelineStepPtr;
typedef std::vector<PipelineStepPtr> PipelineSteps;

template <typename T> PipelineStepPtr makePipelineStep(T &&step) { return std::make_shared<PipelineStep>(std::move(step)); }
} // namespace gfx

#endif // GFX_PIPELINE_STEP

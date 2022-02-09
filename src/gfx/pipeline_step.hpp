#pragma once
#include <vector>

namespace gfx {
struct Feature;
typedef std::shared_ptr<Feature> FeaturePtr;

enum class SortMode {
	// Optimal for batching
	Batch,
	// For transparent object rendering
	BackToFront,
};

struct RenderDrawablesStep {
	std::vector<FeaturePtr> features;
	SortMode sortMode = SortMode::Batch;
};

typedef std::variant<RenderDrawablesStep> PipelineStep;
typedef std::shared_ptr<PipelineStep> PipelineStepPtr;
typedef std::vector<PipelineStepPtr> PipelineSteps;

PipelineStepPtr makeDrawablePipelineStep(RenderDrawablesStep &&step);
} // namespace gfx

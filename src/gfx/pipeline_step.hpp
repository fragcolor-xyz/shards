#pragma once
#include <vector>

namespace gfx {
struct Feature;
typedef std::shared_ptr<Feature> FeaturePtr;

struct RenderDrawablesStep {
	std::vector<FeaturePtr> features;
};

typedef std::variant<RenderDrawablesStep> PipelineStep;
typedef std::shared_ptr<PipelineStep> PipelineStepPtr;
typedef std::vector<PipelineStepPtr> PipelineSteps;

PipelineStepPtr makeDrawablePipelineStep(RenderDrawablesStep &&step);
} // namespace gfx

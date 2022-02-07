#pragma once
#include "gfx_wgpu.hpp"
#include "linalg.hpp"
#include <functional>
#include <memory>

namespace gfx {

struct Context;
struct Pipeline;
struct DrawQueue;

struct Drawable;
typedef std::shared_ptr<Drawable> DrawablePtr;

struct View;
typedef std::shared_ptr<View> ViewPtr;

struct Feature;
typedef std::shared_ptr<Feature> FeaturePtr;

struct RenderDrawablesStep {
	std::vector<FeaturePtr> features;
};

typedef std::variant<RenderDrawablesStep> PipelineStep;
typedef std::shared_ptr<PipelineStep> PipelineStepPtr;
typedef std::vector<PipelineStepPtr> PipelineSteps;

PipelineStepPtr makeDrawablePipelineStep(RenderDrawablesStep &&step);

// Instance that caches render pipelines
struct RendererImpl;
struct Renderer {
	std::shared_ptr<RendererImpl> impl;

	struct MainOutput {
		WGPUTextureView view;
		WGPUTextureFormat format;
		int2 size;
	};

public:
	Renderer(Context &context);
	void render(const DrawQueue &drawQueue, ViewPtr view, const PipelineSteps &pipelineSteps);
	void setMainOutput(const MainOutput &output);

	// Call before frame rendering to swap buffers
	void swapBuffers();

	// Flushes rendering and cleans up all cached data kept by the renderer
	void cleanup();
};

} // namespace gfx

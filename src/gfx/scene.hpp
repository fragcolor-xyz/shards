#pragma once

#include "frame.hpp"
#include "material.hpp"
#include "material_shader.hpp"
#include <functional>
#include <memory>
namespace gfx {

struct Feature;
typedef std::shared_ptr<Feature> FeaturePtr;
struct FrameRenderer;
struct Light;
struct EnvironmentProbe;
struct Material;
struct SceneRenderer {
	typedef std::function<void(View &view, MaterialUsageContext &, ShaderProgramPtr)> DrawCommandFunction;
	struct DrawCommand {
		MaterialUsageContext materialContext;
		DrawCommandFunction function;

		DrawCommand(MaterialBuilderContext &materialBuilderContext) : materialContext(materialBuilderContext) {}
	};

	FrameRenderer &frame;
	MaterialBuilderContext materialBuilderContext;

	std::vector<std::shared_ptr<Feature>> features;
	std::vector<std::shared_ptr<Light>> lights;
	std::vector<std::shared_ptr<EnvironmentProbe>> environmentProbes;
	std::vector<DrawCommand> drawCommands;

	SceneRenderer(FrameRenderer &frame);
	void reset();
	// Renders precomputed maps (shadows/environment/etc.)
	void runPrecompute();
	void renderView();
	void renderView(const std::vector<FeaturePtr> &features);
	void addLight(std::shared_ptr<Light> light);
	void addProbe(std::shared_ptr<EnvironmentProbe> probe);
	void addDrawCommand(Material material, MaterialUsageFlags::Type meshFlags, DrawCommandFunction drawCommand);
};

} // namespace gfx

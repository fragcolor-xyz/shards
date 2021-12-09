#include "scene.hpp"
#include "bgfx/bgfx.h"
#include "env_probe.hpp"
#include "feature.hpp"
#include "linalg/linalg.h"
#include "material_shader.hpp"
#include "math.hpp"
#include <list>
#include <memory>
#include <vector>

namespace gfx {

SceneRenderer::SceneRenderer(Context &context) : context(context) {
}

void SceneRenderer::reset() {
	drawCommands.clear();
}

void SceneRenderer::runPrecompute() {
	for (auto &feature : features) {
		feature->runPrecompute(*this);
	}
}

void SceneRenderer::renderView() { renderView(features); }

void SceneRenderer::renderView(const std::vector<FeaturePtr> &features) {
	for (auto &feature : features) {
		feature->setupScene(*this);
	}

	for (auto &cmd : drawCommands) {
		for (auto &feature : features) {
			feature->setupMaterial(*this, cmd.materialContext);
		}

		ShaderProgramPtr program = cmd.materialContext.getProgram();

		for (auto &feature : features) {
			feature->setupState(*this, cmd.materialContext);
		}

		cmd.function(getFrameRenderer().getCurrentView(), cmd.materialContext, program);
	}
}

void render();
void render(std::vector<FeaturePtr> features);
void SceneRenderer::addDrawCommand(Material material, MaterialUsageFlags::Type meshFlags, DrawCommandFunction function) {
	auto &drawCommand = drawCommands.emplace_back(getMaterialBuilderContext());
	drawCommand.materialContext.material = material;
	drawCommand.materialContext.staticOptions.usageFlags = meshFlags;
	drawCommand.function = function;
}

MaterialBuilderContext &SceneRenderer::getMaterialBuilderContext() { return *context.materialBuilderContext.get(); }
FrameRenderer &SceneRenderer::getFrameRenderer() { return *FrameRenderer::get(context); }

} // namespace gfx

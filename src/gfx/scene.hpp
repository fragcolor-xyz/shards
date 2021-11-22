#pragma once

#include "frame.hpp"
#include "material.hpp"
#include "material_shader.hpp"
#include <functional>
#include <memory>
namespace gfx {

struct FrameRenderer;
struct Light;
struct Material;
struct SceneRenderer {
	typedef std::function<void(View &view, MaterialUsageContext &)> DrawCommandFunction;
	struct DrawCommand {
		MaterialUsageContext materialContext;
		DrawCommandFunction function;

		DrawCommand(MaterialBuilderContext &materialBuilderContext) : materialContext(materialBuilderContext) {}
	};

	FrameRenderer &frame;
	MaterialBuilderContext materialBuilderContext;

	std::vector<std::shared_ptr<Light>> lights;
	std::vector<DrawCommand> drawCommands;

	SceneRenderer(FrameRenderer &frame);
	void begin();
	void end();
	void addLight(std::shared_ptr<Light> light);
	void addProbe();
	void addDrawCommand(Material material, DrawCommandFunction drawCommand);
};

} // namespace gfx

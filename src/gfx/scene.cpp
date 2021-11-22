#include "scene.hpp"
#include "bgfx/bgfx.h"
#include "light.hpp"
#include "material_shader.hpp"
#include <list>
#include <vector>

namespace gfx {

struct DirectionalLightData {
	float color[4];
	float directionIntensity[4];
};

struct DirectionalLightsFeature {
	bgfx::UniformHandle u_directionalLightData;
	std::vector<DirectionalLight *> directionalLights;

	void setup(SceneRenderer &sceneRenderer) {
		for (auto &light : sceneRenderer.lights) {
			if (DirectionalLight *dirLight = dynamic_cast<DirectionalLight *>(light.get())) {
				directionalLights.push_back(dirLight);
			}
		}

		std::vector<DirectionalLightData> directionalLightData;
		for (DirectionalLight *light : directionalLights) {
			DirectionalLightData &data = directionalLightData.emplace_back();
			memcpy(data.color, &light->color, sizeof(float) * 4);
			memcpy(data.directionIntensity, &light->direction, sizeof(float) * 3);
			data.directionIntensity[3] = light->intensity;
		}

		size_t numDirectionalLightVecs = 2 * directionalLights.size();
		u_directionalLightData = bgfx::createUniform("u_dirLightData", bgfx::UniformType::Vec4, numDirectionalLightVecs);
		bgfx::setUniform(u_directionalLightData, directionalLightData.data(), numDirectionalLightVecs);
	}

	void prepareDraw(MaterialUsageContext &context) { context.staticOptions.numDirectionLights = directionalLights.size(); }
};

struct PointLightData {
	float colorInnerRadius[4];
	float positionIntensity[4];
	float outerRadius[4];
};

struct PointLightsFeature {
	bgfx::UniformHandle u_pointLightData;
	std::vector<PointLight *> pointLights;

	void setup(SceneRenderer &sceneRenderer) {
		for (auto &light : sceneRenderer.lights) {
			if (PointLight *dirLight = dynamic_cast<PointLight *>(light.get())) {
				pointLights.push_back(dirLight);
			}
		}

		std::vector<PointLightData> pointLightData;
		for (PointLight *light : pointLights) {
			PointLightData &data = pointLightData.emplace_back();
			memcpy(data.colorInnerRadius, &light->color, sizeof(float) * 3);
			data.colorInnerRadius[3] = light->innerRadius;
			memcpy(data.positionIntensity, &light->position, sizeof(float) * 3);
			data.positionIntensity[3] = light->intensity;
			data.outerRadius[0] = light->outerRadius;
		}

		size_t numPointLightVecs = 3 * pointLights.size();
		u_pointLightData = bgfx::createUniform("u_pointLightData", bgfx::UniformType::Vec4, numPointLightVecs);
		bgfx::setUniform(u_pointLightData, pointLightData.data(), numPointLightVecs);
	}

	void prepareDraw(MaterialUsageContext &context) { context.staticOptions.numPointLights = pointLights.size(); }
};

SceneRenderer::SceneRenderer(FrameRenderer &frame) : frame(frame), materialBuilderContext(frame.context) {}
void SceneRenderer::begin() { lights.clear(); }
void SceneRenderer::end() {
	DirectionalLightsFeature dirLights;
	PointLightsFeature pointLights;
	dirLights.setup(*this);
	pointLights.setup(*this);

	for (auto &cmd : drawCommands) {
		dirLights.prepareDraw(cmd.materialContext);
		pointLights.prepareDraw(cmd.materialContext);
		cmd.function(frame.getCurrentView(), cmd.materialContext);
	}
}
void SceneRenderer::addLight(std::shared_ptr<Light> light) { lights.push_back(light); }
void SceneRenderer::addProbe() {}
void SceneRenderer::addDrawCommand(Material material, std::function<void(View &view, MaterialUsageContext &)> function) {
	auto &drawCommand = drawCommands.emplace_back(materialBuilderContext);
	drawCommand.materialContext.material = material;
	drawCommand.function = function;
}

} // namespace gfx

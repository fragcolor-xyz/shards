#pragma once

#include "feature.hpp"
#include "light.hpp"

namespace gfx {

struct DirectionalLightsFeature : public Feature {
	bgfx::UniformHandle u_directionalLightData;
	std::vector<DirectionalLight *> directionalLights;

	void setupScene(SceneRenderer &scene) override;
	void setupMaterial(SceneRenderer &scene, MaterialUsageContext &context) override;
};

struct PointLightsFeature : public Feature {
	bgfx::UniformHandle u_pointLightData;
	std::vector<PointLight *> pointLights;

	void setupScene(SceneRenderer &scene) override;
	void setupMaterial(SceneRenderer &scene, MaterialUsageContext &context) override;
};

struct EnvironmentProbe;
struct EnvironmentProbeFeature : public Feature {
	bgfx::UniformHandle u_envLambertTexture;
	bgfx::UniformHandle u_envGGXTexture;
	bgfx::UniformHandle u_envGGXLUTTexture;
	bgfx::UniformHandle u_envNumMipLevels;

	TexturePtr ggxLUTTexture;
	std::shared_ptr<EnvironmentProbe> firstEnvProbe;

	void runPrecompute(SceneRenderer &scene) override;
	void setupScene(SceneRenderer &scene) override;
	void setupMaterial(SceneRenderer &scene, MaterialUsageContext &context) override;
	void setupState(SceneRenderer &scene, MaterialUsageContext &context) override;

private:
	void precomputeGGXLUT(SceneRenderer &scene);
	void precomputeEnvironmentProbe(SceneRenderer &scene, std::shared_ptr<EnvironmentProbe> probe);
};
} // namespace gfx

#pragma once

#include "light.hpp"
#include "texture.hpp"
#include <bgfx/bgfx.h>
#include <memory>

namespace gfx {

struct SceneRenderer;
struct MaterialUsageContext;
struct Feature {
	virtual ~Feature() = default;
	virtual void runPrecompute(SceneRenderer &scene) {}
	virtual void setupScene(SceneRenderer &scene) {}
	virtual void setupMaterial(SceneRenderer &scene, MaterialUsageContext &context) {}
	virtual void setupState(SceneRenderer &scene, MaterialUsageContext &context) {}
};

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

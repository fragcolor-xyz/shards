#pragma once

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

} // namespace gfx

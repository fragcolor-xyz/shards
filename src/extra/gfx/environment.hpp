#pragma once

#include "texture.hpp"

namespace gfx {

struct ProbeSharedLightingParameters {
	TexturePtr ggxLUTTexture;
};

struct ProbeLightingParameters {
	TexturePtr lambertTexture;
	TexturePtr ggxTexture;
	float exposure;
};

struct EnvironmentMap {
public:
	EnvironmentMap(TexturePtr texture);
	void rebuild();
	bool isBuilt() const;

private:
	ProbeLightingParameters parameters;
};

} // namespace gfx

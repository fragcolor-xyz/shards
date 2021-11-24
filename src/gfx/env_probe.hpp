#pragma once
#include "texture.hpp"

namespace gfx {

struct EnvironmentProbe {
	TexturePtr lambertTexture;
	TexturePtr ggxTexture;

	EnvironmentProbe() = default;
	bool needsRebuild() const { return !lambertTexture || !ggxTexture; }
};

} // namespace gfx

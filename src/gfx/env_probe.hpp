#pragma once
#include "texture.hpp"

namespace gfx {

struct EnvironmentProbe {
	TexturePtr lambertTexture;
	TexturePtr ggxTexture;
	size_t numMips = 0;

	EnvironmentProbe() = default;
	bool needsRebuild() const { return !lambertTexture || !ggxTexture; }
};

} // namespace gfx

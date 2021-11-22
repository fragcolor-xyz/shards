#pragma once
#include "bgfx/bgfx.h"
#include "texture.hpp"
#include "view.hpp"
#include "view_framebuffer.hpp"
#include <initializer_list>
#include <vector>

namespace gfx {

struct ViewFrameBufferTextureInfo {
	float scale = 1.0f;
	bgfx::TextureFormat::Enum format;
	bool cpuReadBack = false;
};

struct ViewFrameBuffer {
private:
	std::vector<ViewFrameBufferTextureInfo> textureInfos;
	std::vector<TexturePtr> textures;
	FrameBufferPtr frameBuffer;

	int2 referenceSize;

public:
	ViewFrameBuffer(const std::initializer_list<ViewFrameBufferTextureInfo> &textureInfos);
	FrameBufferPtr begin(const View &view);
	void rebuild();
	FrameBufferPtr getFrameBuffer() const { return frameBuffer; }
	TexturePtr getTexture(int index) const;
};

} // namespace gfx

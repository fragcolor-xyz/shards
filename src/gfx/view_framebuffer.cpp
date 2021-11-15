#include "view_framebuffer.hpp"
#include "bgfx/defines.h"
#include "texture.hpp"
#include <memory>

namespace gfx {

ViewFrameBuffer::ViewFrameBuffer(const std::initializer_list<ViewFrameBufferTextureInfo> &textureInfos) : textureInfos(textureInfos) {}

FrameBufferPtr ViewFrameBuffer::begin(const View &view) {
	int2 targetSize = view.getSize();
	if (referenceSize != targetSize) {
		referenceSize = targetSize;
		rebuild();
	}
	return getFrameBuffer();
}

void ViewFrameBuffer::rebuild() {
	textures.clear();
	std::vector<FrameBufferAttachment> attachments;
	for (auto info : textureInfos) {
		int2 textureSize = int2(float2(referenceSize) * info.scale);
		uint64_t flags = BGFX_TEXTURE_RT;
		TexturePtr texture = std::make_shared<Texture>(textureSize, info.format, flags);
		textures.push_back(texture);
		attachments.push_back(texture);
	}
	frameBuffer = std::make_shared<FrameBuffer>(attachments);
}

TexturePtr ViewFrameBuffer::getTexture(int index) const {
	if (textures.size() <= size_t(index))
		return TexturePtr();
	return textures[index];
}

} // namespace gfx

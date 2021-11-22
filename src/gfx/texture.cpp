#include "texture.hpp"
#include "bgfx/bgfx.h"
#include <vector>

namespace gfx {

Texture::Texture(int2 size, bgfx::TextureFormat::Enum format, uint64_t flags) : size(size), format(format) { handle = bgfx::createTexture2D(size.x, size.y, false, 1, format, flags); }
Texture::~Texture() {
	if (bgfx::isValid(handle)) {
		bgfx::destroy(handle);
	}
}
void Texture::update(const bgfx::Memory *memory, int mipLevel) { bgfx::updateTexture2D(handle, 0, mipLevel, 0, 0, size.x, size.y, memory); }

FrameBufferAttachment::FrameBufferAttachment(bgfx::Attachment &&bgfxAttachment) : bgfxAttachment(bgfxAttachment) {}
FrameBufferAttachment::FrameBufferAttachment(TexturePtr texture) : texture(texture) {}

FrameBuffer::~FrameBuffer() {
	if (bgfx::isValid(handle)) {
		bgfx::destroy(handle);
	}
}

} // namespace gfx

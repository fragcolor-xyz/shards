#include "texture.hpp"
#include "bgfx/bgfx.h"
#include <vector>

namespace gfx {

Texture::Texture(int2 size, bgfx::TextureFormat::Enum format, uint64_t flags, bool mips) : size(size), format(format) {
	handle = bgfx::createTexture2D(size.x, size.y, mips, 1, format, flags);
}
Texture::Texture(int2 size, bgfx::TextureFormat::Enum format, bgfx::TextureHandle handle, TextureType type) : handle(handle), size(size), format(format), type(type) {}
Texture::~Texture() {
	if (bgfx::isValid(handle)) {
		bgfx::destroy(handle);
	}
}
void Texture::update(const bgfx::Memory *memory, int mipLevel) { bgfx::updateTexture2D(handle, 0, mipLevel, 0, 0, size.x, size.y, memory); }

FrameBufferAttachment::FrameBufferAttachment(bgfx::Attachment &&bgfxAttachment) : bgfxAttachment(bgfxAttachment) {}
FrameBufferAttachment::FrameBufferAttachment(const bgfx::Attachment &bgfxAttachment) : bgfxAttachment(bgfxAttachment) {}
FrameBufferAttachment::FrameBufferAttachment(TexturePtr texture) : texture(texture) {}

FrameBuffer::~FrameBuffer() {
	if (bgfx::isValid(handle)) {
		bgfx::destroy(handle);
	}
}

} // namespace gfx

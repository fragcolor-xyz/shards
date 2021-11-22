#pragma once

#include "bgfx/bgfx.h"
#include "linalg.hpp"
#include <bgfx/bgfx.h>
#include <memory>
#include <optional>
#include <stdint.h>
#include <utility>
#include <vector>

namespace gfx {
struct Texture {
	bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;

private:
	int2 size;
	bgfx::TextureFormat::Enum format;

public:
	Texture(int2 size, bgfx::TextureFormat::Enum format, uint64_t flags = 0);
	~Texture();

	bgfx::TextureFormat::Enum getFormat() const { return format; }
	int2 getSize() const { return size; }

	void update(const bgfx::Memory *memory, int mipLevel = 0);
};
using TexturePtr = std::shared_ptr<Texture>;

struct FrameBufferAttachment {
	bgfx::Attachment bgfxAttachment;
	TexturePtr texture;

	FrameBufferAttachment(bgfx::Attachment &&bgfxAttachment);
	FrameBufferAttachment(TexturePtr texture);
};

struct FrameBuffer {
	bgfx::FrameBufferHandle handle = BGFX_INVALID_HANDLE;

private:
	std::vector<TexturePtr> referencedTextures;

public:
	FrameBuffer(const std::vector<FrameBufferAttachment> &attachments) { construct(attachments.begin(), attachments.end()); }
	FrameBuffer(const std::initializer_list<FrameBufferAttachment> &attachments) { construct(attachments.begin(), attachments.end()); }

	~FrameBuffer();

private:
	template <typename TIter> void construct(TIter begin, TIter end) {
		std::vector<bgfx::Attachment> bgfxAttachments;
		for (auto it = begin; it != end; ++it) {
			const FrameBufferAttachment& attachment = *it;
			if (attachment.texture) {
				bgfx::Attachment bgfxAttachment;
				bgfxAttachment.init(attachment.texture->handle, bgfx::Access::Write);
				referencedTextures.push_back(attachment.texture);
				bgfxAttachments.push_back(bgfxAttachment);
			} else {
				bgfxAttachments.push_back(attachment.bgfxAttachment);
			}
		}
		handle = bgfx::createFrameBuffer(bgfxAttachments.size(), bgfxAttachments.data(), false);
	}
};
using FrameBufferPtr = std::shared_ptr<FrameBuffer>;
} // namespace gfx

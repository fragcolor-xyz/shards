#pragma once

#include <stdint.h>
#include <bgfx/bgfx.h>
#include <utility>
#include <memory>

namespace gfx {
struct Texture {
	uint16_t width;
	uint16_t height;
	bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;

	Texture(uint16_t width, uint16_t height) : width(width), height(height) {}

	Texture(Texture&& other) {
		std::swap(width, other.width);
		std::swap(height, other.height);
		std::swap(handle, other.handle);
	}

	~Texture() {
		if (handle.idx != bgfx::kInvalidHandle) {
			bgfx::destroy(handle);
		}
	}
};

using TexturePtr = std::shared_ptr<Texture>;
} // namespace gfx

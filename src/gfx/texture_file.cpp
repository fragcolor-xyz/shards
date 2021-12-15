#include "texture_file.hpp"
#include "bgfx/bgfx.h"
#include "bimg/bimg.h"
#include "bx/error.h"
#include "bx/file.h"
#include "bx/readerwriter.h"
#include "bx/string.h"
#include "spdlog/spdlog.h"
#include "texture.hpp"
#include <bgfx/bgfx.h>
// #include "bimg/bimg.h"
// #include <bimg/bimg.h>
#include <bimg/decode.h>
#include <memory>
#include <spdlog/spdlog.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include "stb_image.h"
#pragma GCC diagnostic pop

namespace bgfx {
bx::AllocatorI *g_allocator;
}

namespace gfx {
TexturePtr textureFromFileFloat(const char *path) {
	bx::FileReader reader;
	if (!bx::open(&reader, path)) {
		spdlog::error("Could not open image file path {}", path);
		return TexturePtr();
	}

	std::vector<uint8_t> data;
	data.resize(reader.seek(0, bx::Whence::End));
	reader.seek(0, bx::Whence::Begin);

	bx::Error error;
	reader.read(data.data(), data.size(), &error);
	bx::close(&reader);

	bimg::ImageContainer *imageContainer = bimg::imageParse(bgfx::g_allocator, data.data(), data.size(), bimg::TextureFormat::RGBA32F, &error);
	if (!imageContainer) {
		bx::StringView errorStringView = error.getMessage();
		spdlog::error("Could not parse image at path {}: {}", path, std::string(errorStringView.getPtr(), errorStringView.getTerm()));
		return TexturePtr();
	}

	const bgfx::Memory *memory = bgfx::copy(imageContainer->m_data, imageContainer->m_size);
	TexturePtr texture =
		std::make_shared<Texture>(int2(imageContainer->m_width, imageContainer->m_height), (bgfx::TextureFormat::Enum)imageContainer->m_format);
	texture->update(memory);

	bimg::imageFree(imageContainer);

	return texture;
}
} // namespace gfx

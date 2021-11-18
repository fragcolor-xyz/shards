#pragma once

#include <bgfx/bgfx.h>
#include <memory>
#include <stdint.h>
#include <utility>

namespace gfx {
struct Texture {
	bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;

  private:
	uint16_t width;
	uint16_t height;

  public:
	Texture(uint16_t width, uint16_t height) : width(width), height(height) {}

	Texture(Texture &&other) {
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

// struct Texture {
// 	static inline Type ObjType{{CBType::Object, {.object = {.vendorId = CoreCC, .typeId = BgfxTextureHandleCC}}}};
// 	static inline Type SeqType = Type::SeqOf(ObjType);
// 	static inline Type VarType = Type::VariableOf(ObjType);
// 	static inline Type VarSeqType = Type::VariableOf(SeqType);

// 	static inline ObjectVar<Texture> Var{"BGFX-Texture", CoreCC, BgfxTextureHandleCC};

// 	bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
// 	uint16_t width = 0;
// 	uint16_t height = 0;
// 	uint8_t channels = 0;
// 	int bpp = 1;

// 	void init(uint32_t width, uint32_t height, bgfx::TextureFormat::Enum format);

// 	~Texture() {
// 		if (handle.idx != bgfx::kInvalidHandle) {
// 			bgfx::destroy(handle);
// 		}
// 	}
// };

// utility macro to load textures of different sizes
// #define BGFX_TEXTURE2D_CREATE(_bits, _components, _texture)
// 	if (_bits == 8) {
// 		switch (_components) {
// 		case 1:
// 			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::R8);
// 			break;
// 		case 2:
// 			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RG8);
// 			break;
// 		case 3:
// 			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RGB8);
// 			break;
// 		case 4:
// 			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RGBA8);
// 			break;
// 		default:
// 			CBLOG_FATAL("invalid state");
// 			break;
// 		}
// 	}
// 	else if (_bits == 16) {
// 		switch (_components) {
// 		case 1:
// 			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::R16);
// 			break;
// 		case 2:
// 			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RG16);
// 			break;
// 		case 3:
// 			throw ActivationError(
// 				"Format not supported, it seems bgfx has no "
// 				"RGB16, try using RGBA16 instead (FillAlpha).");
// 			break;
// 		case 4:
// 			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RGBA16);
// 			break;
// 		default:
// 			CBLOG_FATAL("invalid state");
// 			break;
// 		}
// 	}
// 	else if (_bits == 32) {
// 		switch (_components) {
// 		case 1:
// 			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::R32F);
// 			break;
// 		case 2:
// 			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RG32F);
// 			break;
// 		case 3:
// 			throw ActivationError(
// 				"Format not supported, it seems bgfx has no RGB32F, try using "
// 				"RGBA32F instead (FillAlpha).");
// 			break;
// 		case 4:
// 			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RGBA32F);
// 			break;
// 		default:
// 			CBLOG_FATAL("invalid state");
// 			break;
// 		}
// 	}

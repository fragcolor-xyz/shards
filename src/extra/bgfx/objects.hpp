#pragma once

#include "bgfx/bgfx.h"
#include "blocks/shared.hpp"
#include "enums.hpp"
#include "linalg_shim.hpp"

using namespace chainblocks;

namespace BGFX {

constexpr uint32_t BgfxTextureHandleCC = 'gfxT';
constexpr uint32_t BgfxShaderHandleCC = 'gfxS';
constexpr uint32_t BgfxModelHandleCC = 'gfxM';

struct Texture {
	static inline Type ObjType{{CBType::Object, {.object = {.vendorId = CoreCC, .typeId = BgfxTextureHandleCC}}}};
	static inline Type SeqType = Type::SeqOf(ObjType);
	static inline Type VarType = Type::VariableOf(ObjType);
	static inline Type VarSeqType = Type::VariableOf(SeqType);

	static inline ObjectVar<Texture> Var{"BGFX-Texture", CoreCC, BgfxTextureHandleCC};

	bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
	uint16_t width = 0;
	uint16_t height = 0;
	uint8_t channels = 0;
	int bpp = 1;

	~Texture() {
		if (handle.idx != bgfx::kInvalidHandle) {
			bgfx::destroy(handle);
		}
	}
};

// utility macro to load textures of different sizes
#define BGFX_TEXTURE2D_CREATE(_bits, _components, _texture)                                                                      \
	if (_bits == 8) {                                                                                                            \
		switch (_components) {                                                                                                   \
		case 1:                                                                                                                  \
			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::R8);      \
			break;                                                                                                               \
		case 2:                                                                                                                  \
			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RG8);     \
			break;                                                                                                               \
		case 3:                                                                                                                  \
			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RGB8);    \
			break;                                                                                                               \
		case 4:                                                                                                                  \
			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RGBA8);   \
			break;                                                                                                               \
		default:                                                                                                                 \
			CBLOG_FATAL("invalid state");                                                                                        \
			break;                                                                                                               \
		}                                                                                                                        \
	}                                                                                                                            \
	else if (_bits == 16) {                                                                                                      \
		switch (_components) {                                                                                                   \
		case 1:                                                                                                                  \
			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::R16);     \
			break;                                                                                                               \
		case 2:                                                                                                                  \
			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RG16);    \
			break;                                                                                                               \
		case 3:                                                                                                                  \
			throw ActivationError(                                                                                               \
				"Format not supported, it seems bgfx has no "                                                                    \
				"RGB16, try using RGBA16 instead (FillAlpha).");                                                                 \
			break;                                                                                                               \
		case 4:                                                                                                                  \
			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RGBA16);  \
			break;                                                                                                               \
		default:                                                                                                                 \
			CBLOG_FATAL("invalid state");                                                                                        \
			break;                                                                                                               \
		}                                                                                                                        \
	}                                                                                                                            \
	else if (_bits == 32) {                                                                                                      \
		switch (_components) {                                                                                                   \
		case 1:                                                                                                                  \
			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::R32F);    \
			break;                                                                                                               \
		case 2:                                                                                                                  \
			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RG32F);   \
			break;                                                                                                               \
		case 3:                                                                                                                  \
			throw ActivationError(                                                                                               \
				"Format not supported, it seems bgfx has no RGB32F, try using "                                                  \
				"RGBA32F instead (FillAlpha).");                                                                                 \
			break;                                                                                                               \
		case 4:                                                                                                                  \
			_texture->handle = bgfx::createTexture2D(_texture->width, _texture->height, false, 1, bgfx::TextureFormat::RGBA32F); \
			break;                                                                                                               \
		default:                                                                                                                 \
			CBLOG_FATAL("invalid state");                                                                                        \
			break;                                                                                                               \
		}                                                                                                                        \
	}

struct ShaderHandle {
	static inline Type ObjType{{CBType::Object, {.object = {.vendorId = CoreCC, .typeId = BgfxShaderHandleCC}}}};
	static inline Type VarType = Type::VariableOf(ObjType);

	static inline ObjectVar<ShaderHandle> Var{"BGFX-Shader", CoreCC, BgfxShaderHandleCC};

	bgfx::ProgramHandle handle = BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle pickingHandle = BGFX_INVALID_HANDLE;

	~ShaderHandle() {
		if (handle.idx != bgfx::kInvalidHandle) {
			bgfx::destroy(handle);
		}

		if (pickingHandle.idx != bgfx::kInvalidHandle) {
			bgfx::destroy(pickingHandle);
		}
	}
};

struct ModelHandle {
	static inline Type ObjType{{CBType::Object, {.object = {.vendorId = CoreCC, .typeId = BgfxModelHandleCC}}}};
	static inline Type VarType = Type::VariableOf(ObjType);

	static inline ObjectVar<ModelHandle> Var{"BGFX-Model", CoreCC, BgfxModelHandleCC};

	struct StaticModel {
		bgfx::VertexBufferHandle vb;
		bgfx::IndexBufferHandle ib;
	};

	struct DynamicModel {
		bgfx::DynamicVertexBufferHandle vb;
		bgfx::DynamicIndexBufferHandle ib;
	};

	uint64_t topologyStateFlag{0}; // Triangle list
	Enums::CullMode cullMode{Enums::CullMode::Back};

	std::variant<StaticModel, DynamicModel> model{StaticModel{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};

	void reset() {
		std::visit(
			[](auto &m) {
				if (m.vb.idx != bgfx::kInvalidHandle) {
					bgfx::destroy(m.vb);
				}
				if (m.ib.idx != bgfx::kInvalidHandle) {
					bgfx::destroy(m.ib);
				}
			},
			model);
	}

	~ModelHandle() { reset(); }
};

struct PointLight {
	Vec3 position = Vec3(0, 0, 0);
	Vec3 color = Vec3(1, 1, 1);
	float innerRadius = 5.0f;
	float outerRadius = 25.0f;
};

} // namespace BGFX

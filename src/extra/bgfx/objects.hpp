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

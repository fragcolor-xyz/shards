#include "enums.hpp"
#include <magic_enum.hpp>

namespace gfx {

bgfx::RendererType::Enum getBgfxRenderType(RendererType renderer) {
	switch (renderer) {
	case RendererType::DirectX11:
		return bgfx::RendererType::Direct3D11;
	case RendererType::Vulkan:
		return bgfx::RendererType::Vulkan;
	case RendererType::OpenGL:
		return bgfx::RendererType::OpenGL;
	case RendererType::Metal:
		return bgfx::RendererType::Metal;
	default:
		return bgfx::RendererType::Noop;
	}
}

std::string_view getRendererTypeName(RendererType renderer) { return magic_enum::enum_name(renderer); }
RendererType getRendererTypeByName(std::string_view inName) {
	size_t index = 0;
	for (const auto &name : magic_enum::enum_names<RendererType>()) {
		if (name == inName) {
			return (RendererType)index;
		}
		index++;
	}
	return RendererType::None;
}
} // namespace gfx
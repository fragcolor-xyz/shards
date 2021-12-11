#pragma once

#include <string_view>
#include <bgfx/bgfx.h>

namespace gfx {

enum class PrimitiveType { TriangleList, TriangleStrip };
enum class WindingOrder { CW, CCW };
enum class ProgrammableGraphicsStage { Vertex, Fragment };

enum class RendererType : uint8_t { None, DirectX11, Vulkan, OpenGL, Metal };
std::string_view getRendererTypeName(RendererType renderer);
bgfx::RendererType::Enum getBgfxRenderType(RendererType renderer);
RendererType getRendererTypeByName(std::string_view inName);

} // namespace gfxF

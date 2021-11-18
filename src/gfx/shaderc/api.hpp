#pragma once
#include "../shaderc.hpp"

namespace gfx {

#define GFX_SHADERC_MODULE_ENTRY _shadercCreateModule
typedef IShaderCompiler *(*f_GetShaderCompilerInterface)();

} // namespace gfx
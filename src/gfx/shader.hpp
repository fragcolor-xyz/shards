#pragma once

#include "material.hpp"
#include <bgfx/bgfx.h>
#include <memory>

namespace gfx {
struct ShaderHandle {
	bgfx::ShaderHandle shaderHandle = BGFX_INVALID_HANDLE;
	ShaderHandle(bgfx::ShaderHandle shaderHandle) : shaderHandle(shaderHandle) {}
	~ShaderHandle();
};
using ShaderHandlePtr = std::shared_ptr<ShaderHandle>;

struct ShaderProgram {
	bgfx::ProgramHandle program;

	ShaderProgram(bgfx::ProgramHandle program) : program(program) {}
	~ShaderProgram() {
		if (bgfx::isValid(program))
			bgfx::destroy(program);
	}
};
using ShaderProgramPtr = std::shared_ptr<ShaderProgram>;

struct ShaderBuilder {
	Material material;
	MaterialUsageFlags::Type materialUsageFlags;
};
} // namespace gfx

#pragma once

#include <memory>
#include <unordered_map>
#include "material_internal.hpp"
#include "bgfx/bgfx.h"

struct CBContext;
namespace chainblocks {
struct IShaderCompiler;
}

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
		if (bgfx::isValid(program)) bgfx::destroy(program);
	}
};
using ShaderProgramPtr = std::shared_ptr<ShaderProgram>;

struct ShaderCache {
	struct SourceInfo {
		std::unordered_map<StaticShaderParameters, ShaderHandlePtr> shaders;
	};
	struct MaterialInfo {
		std::unordered_map<StaticUsageParameters, ShaderProgramPtr> programs;
	};
	std::unordered_map<ShaderSource, SourceInfo> sources;
	std::unordered_map<std::shared_ptr<Material>, MaterialInfo> materials;

	ShaderCache() = default;
	ShaderProgramPtr createProgram(CBContext& context, chainblocks::IShaderCompiler& shaderCompiler, const StaticShaderParameters& staticShaderParameters);
	ShaderProgramPtr getOrCreateProgram(
		CBContext& context, chainblocks::IShaderCompiler& shaderCompiler, std::shared_ptr<Material> material, const StaticUsageParameters& usage);

	void clear();
};
} // namespace gfx

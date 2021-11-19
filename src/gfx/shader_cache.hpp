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

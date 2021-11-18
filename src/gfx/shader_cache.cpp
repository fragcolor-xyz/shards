#include "shader_cache.hpp"
#include <boost/algorithm/string/join.hpp>
#include "spdlog/spdlog.h"
#include "../bgfx.hpp"
#include "utility.hpp"

namespace gfx {

ShaderHandle::~ShaderHandle() {
	if (bgfx::isValid(shaderHandle)) bgfx::destroy(shaderHandle);
}

ShaderProgramPtr ShaderCache::createProgram(
	CBContext& context, chainblocks::IShaderCompiler& shaderCompiler, const StaticShaderParameters& staticShaderParameters) {
	Shared<ShaderLibrary> sharedShaderLibrary;
	const ShaderLibrary& shaderLibrary = sharedShaderLibrary.get();

	auto compileShader = [&](const ShaderSource& source) -> ShaderHandlePtr {
		MaterialShaderCompilerInputs compilerInputs = MaterialShaderCompilerInputs::create(staticShaderParameters);

		std::string type = source.type == ShaderSourceType::Vertex ? "v" : "f";
		std::string definesString = boost::algorithm::join(compilerInputs.defines, ";");

		try {
			CBVar compiledShader = shaderCompiler.compile(source.varyings, source.source, type, definesString, &context);

			const bgfx::Memory* shaderBinary = bgfx::copy(compiledShader.payload.bytesValue, compiledShader.payload.bytesSize);
			bgfx::ShaderHandle shaderHandle = bgfx::createShader(shaderBinary);
			if (!bgfx::isValid(shaderHandle)) return ShaderHandlePtr();

			return std::make_shared<ShaderHandle>(shaderHandle);

		} catch (CBException& ex) {
			return ShaderHandlePtr();
		}
	};

	auto getOrCompileCached = [&](const ShaderSourceType Type) -> ShaderHandlePtr {
		ShaderSource source = shaderLibrary.getShaderSource(Type);
		auto it = sources.find(source);
		if (it != sources.end()) {
			auto& shaders = it->second.shaders;
			auto it1 = shaders.find(staticShaderParameters);
			if (it1 != shaders.end()) {
				return it1->second;
			}
		}

		ShaderHandlePtr shader = compileShader(source);
		auto& shadersForSource = sources[source].shaders;
		shadersForSource.insert_or_assign(staticShaderParameters, shader);

		return shader;
	};

	ShaderHandlePtr vertexShader = getOrCompileCached(ShaderSourceType::Vertex);
	ShaderHandlePtr pixelShader = getOrCompileCached(ShaderSourceType::Pixel);
	if (!vertexShader || !pixelShader) {
		return ShaderProgramPtr();
	}

	return std::make_shared<ShaderProgram>(bgfx::createProgram(vertexShader->shaderHandle, pixelShader->shaderHandle, false));
}

ShaderProgramPtr ShaderCache::getOrCreateProgram(
	CBContext& context, chainblocks::IShaderCompiler& shaderCompiler, std::shared_ptr<Material> material, const StaticUsageParameters& usage) {
	if (!material) return ShaderProgramPtr();

	MaterialInfo& materialInfo = materials[material];

	auto it = materialInfo.programs.find(usage);
	if (it == materialInfo.programs.end()) {
		StaticShaderParameters params;
		params.usage = usage;
		params.material = material->getStaticMaterialParameters();

		ShaderProgramPtr program = createProgram(context, shaderCompiler, params);
		materialInfo.programs[usage] = program;
		return program;
	}
	return it->second;
}

void ShaderCache::clear() {
	sources.clear();
	materials.clear();
}
} // namespace gfx

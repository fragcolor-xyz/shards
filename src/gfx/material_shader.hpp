#pragma once

#include "bgfx/bgfx.h"
#include "linalg/linalg.h"
#include "material.hpp"
#include "texture.hpp"
#include <bgfx/bgfx.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gfx {
struct ShaderHandle {
	bgfx::ShaderHandle shaderHandle = BGFX_INVALID_HANDLE;
	ShaderHandle(bgfx::ShaderHandle shaderHandle) : shaderHandle(shaderHandle) {}
	~ShaderHandle();
};
using ShaderHandlePtr = std::shared_ptr<ShaderHandle>;

struct ShaderProgram {
	bgfx::ProgramHandle handle = BGFX_INVALID_HANDLE;

public:
	ShaderProgram(bgfx::ProgramHandle handle) : handle(handle) {}
	~ShaderProgram() {
		if (bgfx::isValid(handle))
			bgfx::destroy(handle);
	}
};
using ShaderProgramPtr = std::shared_ptr<ShaderProgram>;

struct Context;
struct MaterialBuilderContext {
	Context &context;
	TexturePtr fallbackTexture;
	ShaderProgramPtr fallbackShaderProgram;

	std::unordered_map<std::string, float4> defaultVectorParameters;
	std::unordered_map<std::string, bgfx::UniformHandle> uniformsCache;

	MaterialBuilderContext(Context &context);
	~MaterialBuilderContext();
	MaterialBuilderContext(const MaterialBuilderContext &) = delete;
	MaterialBuilderContext &operator=(const MaterialBuilderContext &) = delete;

	bgfx::UniformHandle getUniformHandle(const std::string &name, bgfx::UniformType::Enum type, uint32_t num = 1);

	TexturePtr createFallbackTexture();
	ShaderProgramPtr createFallbackShaderProgram();
};

struct MaterialUsageContext {
	MaterialBuilderContext &context;
	Material material;
	MaterialUsageFlags::Type materialUsageFlags;
	std::unordered_map<std::string, size_t> textureRegisterMap;

	MaterialUsageContext(MaterialBuilderContext &context) : context(context) {}
	ShaderProgramPtr getProgram();
	ShaderProgramPtr compileProgram();
	void bindUniforms();
};
} // namespace gfx

#pragma once

#include "bgfx/bgfx.h"
#include "hash.hpp"
#include "linalg/linalg.h"
#include "material.hpp"
#include "texture.hpp"
#include <bgfx/bgfx.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gfx {
struct Feature;
typedef std::shared_ptr<Feature> FeaturePtr;

struct ShaderHandle {
	bgfx::ShaderHandle handle = BGFX_INVALID_HANDLE;
	ShaderHandle(bgfx::ShaderHandle handle) : handle(handle) {}
	~ShaderHandle() {
		if (bgfx::isValid(handle))
			bgfx::destroy(handle);
	}
};
using ShaderHandlePtr = std::shared_ptr<ShaderHandle>;

struct ShaderProgram {
	bgfx::ProgramHandle handle = BGFX_INVALID_HANDLE;
	std::unordered_map<std::string, size_t> textureRegisterMap;

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

	std::unordered_map<Hash128, ShaderHandlePtr> compiledShaders;
	std::unordered_map<Hash128, ShaderProgramPtr> compiledPrograms;

	MaterialBuilderContext(Context &context);
	~MaterialBuilderContext();
	MaterialBuilderContext(const MaterialBuilderContext &) = delete;
	MaterialBuilderContext &operator=(const MaterialBuilderContext &) = delete;

	ShaderHandlePtr getCachedShader(Hash128 inputHash);
	ShaderProgramPtr getCachedProgram(Hash128 inputHash);

	bgfx::UniformHandle getUniformHandle(const std::string &name, bgfx::UniformType::Enum type, uint32_t num = 1);

private:
	TexturePtr createFallbackTexture();
	ShaderProgramPtr createFallbackShaderProgram();
};

struct MaterialUsageFlags {
	enum Type : uint8_t {
		None = 0,
		HasNormals = 1 << 0,
		HasTangents = 1 << 1,
		HasVertexColors = 1 << 2,
		Instanced = 1 << 3,
		Picking = 1 << 4,
		FlipTexcoordY = 1 << 5,
	};
	static bool contains(const MaterialUsageFlags::Type &a, const MaterialUsageFlags::Type &b) { return (a & b) != 0; }
};

inline MaterialUsageFlags::Type &operator|=(MaterialUsageFlags::Type &a, MaterialUsageFlags::Type b) {
	(uint8_t &)a |= uint8_t(a) | uint8_t(b);
	return a;
}

struct StaticMaterialOptions {
	int numDirectionLights = 0;
	int numPointLights = 0;
	bool hasEnvironmentLight = false;
	MaterialUsageFlags::Type usageFlags = MaterialUsageFlags::None;

	template <typename THash> void hashStatic(THash &hash) const {
		hash(numDirectionLights);
		hash(numPointLights);
		hash(hasEnvironmentLight);
		hash(usageFlags);
	}
};

struct MaterialUsageContext {
	MaterialBuilderContext &context;
	Material material;
	std::unordered_map<std::string, MaterialTextureSlot> textureSlots;
	StaticMaterialOptions staticOptions;

	MaterialUsageContext(MaterialBuilderContext &context) : context(context) {}
	ShaderProgramPtr getProgram();
	ShaderProgramPtr compileProgram();
	void bindUniforms(ShaderProgramPtr program);
	void setState();

	void preloadUniforms();

	template <typename THash> void hashStatic(THash &hash) const {
		hash(material.getStaticHash());
		hash(staticOptions);
		hash(textureSlots);
	}
};
} // namespace gfx

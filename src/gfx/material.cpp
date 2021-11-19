#include "material.hpp"
#include "bgfx/bgfx.h"
#include "hasherxxh128.hpp"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace gfx {

void Material::modified() {
	struct Visitor {
		template <typename T, typename THasher> void operator()(const T &val, THasher& hasher) {
			val.hashStatic(hasher);
		}
	};
	HasherXXH128<Visitor> hasher;
	data.hashStatic(hasher);
	hash = hasher.getDigest();
}

// static bool tryLoadShaderSource(std::string& outSource, const char* path) {
// 	std::ifstream stream(path);
// 	if (!stream.is_open()) return false;
// 	std::stringstream buffer;
// 	buffer << stream.rdbuf();
// 	outSource.assign(buffer.str());
// 	return true;
// }

// ShaderLibrary::ShaderLibrary() {
// 	bool failed = false;
// 	if (!tryLoadShaderSource(varyings, "shaders/lib/gltf/varying.txt")) {
// 		failed = false;
// 	}
// 	if (!tryLoadShaderSource(vsSource, "shaders/lib/gltf/vs_entry.h")) {
// 		failed = false;
// 	}
// 	if (!tryLoadShaderSource(psSource, "shaders/lib/gltf/ps_entry.h")) {
// 		failed = false;
// 	}
// 	if (failed) {
// 		SPDLOG_CRITICAL("shaders library is missing");
// 	}
// }

// MaterialShaderCompilerInputs MaterialShaderCompilerInputs::create(const StaticShaderParameters& params) {
// 	const StaticUsageParameters& usage = params.usage;
// 	const StaticMaterialParameters& material = params.material;

// 	MaterialShaderCompilerInputs result;

// 	auto addTextureParameter = [&](std::optional<TextureParameter> param, const char* key) {
// 		if (param.has_value()) {
// 			std::stringstream defineSs;
// 			defineSs << key << "=" << (int)param->texCoordIndex;
// 			result.defines.push_back(defineSs.str());
// 		}
// 	};
// 	addTextureParameter(material.baseColorTexture, "CB_BASE_TEXTURE");
// 	addTextureParameter(material.metallicRoughnessTexture, "CB_METALLIC_ROUGHNESS_TEXTURE");
// 	addTextureParameter(material.normalTexture, "CB_NORMAL_TEXTURE");
// 	addTextureParameter(material.occlusionTexture, "CB_OCCLUSION_TEXTURE");
// 	addTextureParameter(material.emissiveTexture, "CB_EMISSIVE_TEXTURE");

// 	auto addFlagParameter = [&](MaterialUsageFlags::Type flag, const char* defineStr) {
// 		if ((usage.flags & flag) == flag) {
// 			result.defines.push_back(defineStr);
// 		}
// 	};
// 	addFlagParameter(MaterialUsageFlags::HasNormals, "CB_NORMALS");
// 	addFlagParameter(MaterialUsageFlags::HasTangents, "CB_TANGENTS");
// 	addFlagParameter(MaterialUsageFlags::HasVertexColors, "CB_VERTEX_COLOR");
// 	addFlagParameter(MaterialUsageFlags::Instanced, "CB_INSTANCED");
// 	addFlagParameter(MaterialUsageFlags::Picking, "CB_PICKING");

// 	return result;
// }

// ShaderSource ShaderLibrary::getShaderSource(const ShaderSourceType& type) const {
// 	ShaderSource result;
// 	result.varyings = varyings;
// 	result.type = type;
// 	switch (type) {
// 	case ShaderSourceType::Vertex:
// 		result.source = vsSource;
// 		break;
// 	case ShaderSourceType::Pixel:
// 		result.source = psSource;
// 		break;
// 	}
// 	return result;
// }

} // namespace gfx

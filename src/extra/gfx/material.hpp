#pragma once

#include "stdint.h"
#include "linalg.h"
#include "texture.hpp"
#include <functional>
#include <optional>

namespace gfx {

struct UsageFlags {
	enum Type : uint8_t {
		None = 0x0,
		HasNormals = 1 << 0,
		HasTangents = 1 << 1,
		HasVertexColors = 1 << 2,
		Instanced = 1 << 3,
		Picking = 1 << 4,
	};
	static bool contains(const UsageFlags::Type& a, const UsageFlags::Type& b) { return (a & b) != 0; }
};

inline UsageFlags::Type& operator|=(UsageFlags::Type& a, UsageFlags::Type b) {
	(uint8_t&)a |= uint8_t(a) | uint8_t(b);
	return a;
}

// Data shader compilation depends on influenced by external factors
// for example:
//  - mesh vertex attributes
//  - render mode (picking, depth only, etc.)
//  - debug modes
struct StaticUsageParameters {
	UsageFlags::Type flags = UsageFlags::None;

	StaticUsageParameters() = default;
	StaticUsageParameters(UsageFlags::Type flags) : flags(flags) {}

	bool operator==(const StaticUsageParameters& other) const { return flags == other.flags; }
};

struct TextureParameter {
	uint8_t texCoordIndex = 0;

	TextureParameter(uint8_t texCoordIndex) : texCoordIndex(texCoordIndex) {}
	bool operator==(const TextureParameter& other) const { return texCoordIndex == other.texCoordIndex; }
};

// Data shader compilation depends on influenced by the material
// for example:
// - which textures are being used
struct StaticMaterialParameters {
	std::optional<TextureParameter> baseColorTexture;
	std::optional<TextureParameter> metallicRoughnessTexture;
	std::optional<TextureParameter> normalTexture;
	std::optional<TextureParameter> occlusionTexture;
	std::optional<TextureParameter> emissiveTexture;

	bool operator==(const StaticMaterialParameters& other) const {
		return baseColorTexture == other.baseColorTexture && metallicRoughnessTexture == other.metallicRoughnessTexture &&
			   normalTexture == other.normalTexture && occlusionTexture == other.occlusionTexture && emissiveTexture == other.emissiveTexture;
	}
};

// Combination of internal/external influences on shader compilation
struct StaticShaderParameters {
	StaticUsageParameters usage;
	StaticMaterialParameters material;

	bool operator==(const StaticShaderParameters& other) const { return usage == other.usage && material == other.material; }
};

// Dynamic material parameters
// These are free to change without having to recompile shaders
struct DynamicMaterialParameters {
	using float4 = linalg::aliases::float4;
	using float3 = linalg::aliases::float3;

	float4 baseColorFactor = float4(1, 1, 1, 1);
	TexturePtr baseColorTexture;

	float metallicFactor = 0.0f;
	float roughnessFactor = 1.0f;
	TexturePtr metallicRoughnessTexture;

	TexturePtr normalTexture;
	float normalTextureScale = 1.0f;

	TexturePtr occlusionTexture;
	float occlusionTextureScale = 1.0f;

	float3 emissiveFactor = float3(0, 0, 0);
	TexturePtr emissiveTexture;

	bool doubleSided = false;
};

struct Material {
public:
	DynamicMaterialParameters dynamicMaterialParameters;

private:
	StaticMaterialParameters staticMaterialParameters;

public:
	Material(const StaticMaterialParameters& staticMaterialParameters) : staticMaterialParameters(staticMaterialParameters) {}

	const StaticMaterialParameters& getStaticMaterialParameters() const { return staticMaterialParameters; }
};

} // namespace gfx

namespace std {
template<>
struct hash<gfx::TextureParameter> {
	size_t operator()(const gfx::TextureParameter& v) const { return std::hash<int>{}(v.texCoordIndex); }
};

template<>
struct hash<gfx::StaticMaterialParameters> {
	size_t operator()(const gfx::StaticMaterialParameters& v) const {
		static std::hash<std::optional<gfx::TextureParameter>> textureParamHash;
		size_t hash = textureParamHash(v.baseColorTexture);
		hash = (hash << 1) ^ textureParamHash(v.metallicRoughnessTexture);
		hash = (hash << 1) ^ textureParamHash(v.normalTexture);
		hash = (hash << 1) ^ textureParamHash(v.occlusionTexture);
		hash = (hash << 1) ^ textureParamHash(v.emissiveTexture);
		return hash;
	}
};

template<>
struct hash<gfx::StaticUsageParameters> {
	size_t operator()(const gfx::StaticUsageParameters& v) const { return std::hash<uint8_t>{}((uint8_t&)v.flags); }
};

template<>
struct hash<gfx::StaticShaderParameters> {
	size_t operator()(const gfx::StaticShaderParameters& v) const {
		size_t hash = std::hash<gfx::StaticUsageParameters>{}(v.usage);
		hash = (hash << 1) ^ std::hash<gfx::StaticMaterialParameters>{}(v.material);
		return hash;
	}
};
} // namespace std

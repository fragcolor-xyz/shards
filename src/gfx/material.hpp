#pragma once

#include "hash.hpp"
#include "linalg.hpp"
#include "stdint.h"
#include "texture.hpp"
#include <functional>
#include <optional>
#include <unordered_map>

namespace gfx {

struct MaterialStaticFlags {
	enum Type : uint32_t {
		None = 0,
		Lit = 1 << 0,
	};
};

struct MaterialDynamicFlags {
	enum Type : uint32_t {
		None = 0,
		DoubleSided = 1 << 0,
	};
};

struct MaterialUsageFlags {
	enum Type : uint8_t {
		None = 0,
		HasNormals = 1 << 0,
		HasTangents = 1 << 1,
		HasVertexColors = 1 << 2,
		Instanced = 1 << 3,
		Picking = 1 << 4,
	};
	static bool contains(const MaterialUsageFlags::Type &a, const MaterialUsageFlags::Type &b) { return (a & b) != 0; }
};

inline MaterialUsageFlags::Type &operator|=(MaterialUsageFlags::Type &a, MaterialUsageFlags::Type b) {
	(uint8_t &)a |= uint8_t(a) | uint8_t(b);
	return a;
}

struct MaterialTextureSlot {
	TexturePtr texture;
	uint8_t texCoordIndex = 0;

	template <typename THash> void hashStatic(THash &hash) const { hash(texCoordIndex); }
};

struct MaterialData {
	MaterialStaticFlags::Type flags = MaterialStaticFlags::None;
	std::unordered_map<std::string, float4> vectorParameters;
	std::unordered_map<std::string, MaterialTextureSlot> textureSlots;
	std::string vertexCode;
	std::string pixelCode;

	template <typename THash> void hashStatic(THash &hash) const {
		hash(flags);
		hash(textureSlots);
		hash(vertexCode);
		hash(pixelCode);
	}
};

struct Material {
private:
	MaterialData data;
	Hash128 staticHash;
	Hash128 dynamicHash;

public:
	Material() { modified(); }

	template <typename T> void modify(T &&lambda) {
		lambda(data);
		modified();
	}
	void modified();
	const MaterialData &getData() const { return data; }
	const Hash128 &getHash() const { return hash; }
};

} // namespace gfx

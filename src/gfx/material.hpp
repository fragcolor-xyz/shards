#pragma once

#include "hash.hpp"
#include "linalg.hpp"
#include "stdint.h"
#include "texture.hpp"
#include "feature.hpp"
#include <functional>
#include <optional>
#include <string>
#include <map>

namespace gfx {

struct MaterialTextureSlot {
	TexturePtr texture;
	uint8_t texCoordIndex = 0;

	template <typename THash> void hashStatic(THash &hash) const { hash(texCoordIndex); }
};

struct MaterialData {
	std::map<std::string, FieldVariant> basicParameters;
	std::map<std::string, MaterialTextureSlot> textureParameters;

	const FieldVariant* findBasicParameter(const std::string& name) const {
		auto it = basicParameters.find(name);
		if (it != basicParameters.end())
			return &it->second;
		return nullptr;
	}

	const MaterialTextureSlot *findTextureSlot(const std::string& name) const {
		auto it = textureParameters.find(name);
		if (it != textureParameters.end())
			return &it->second;
		return nullptr;
	}

	template <typename THash> void hashStatic(THash &hash) const {
		for (auto &pair : basicParameters) {
			hash(pair.first);
		}

		for (auto &pair : textureParameters) {
			hash(pair.first);
			hash(pair.second.texCoordIndex);
		}
	}
};

struct MaterialBuiltin {
	static constexpr const char *baseColor = "baseColor";
	static constexpr const char *metallic = "metallic";
	static constexpr const char *roughness = "roughness";
	static constexpr const char *baseColorTexture = "baseColorTexture";
	static constexpr const char *normalTexture = "normalTexture";
	static constexpr const char *metalicRoughnessTexture = "metalicRoughnessTexture";
	static constexpr const char *emissiveTexture = "emissiveTexture";
};

struct Material {
public:
	std::vector<FeaturePtr> customFeatures;

private:
	MaterialData data;
	Hash128 staticHash;

public:
	Material() { modified(); }

	template <typename T> void modify(T &&lambda) {
		lambda(data);
		modified();
	}
	void modified();
	const MaterialData &getData() const { return data; }
	const Hash128 &getStaticHash() const { return staticHash; }
};

} // namespace gfx

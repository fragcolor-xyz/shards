#pragma once

#include "fwd.hpp"
#include "params.hpp"
#include <map>
#include <memory>

namespace gfx {

// Container for shader parameters (basic/texture)
struct MaterialParameters {
	std::map<std::string, ParamVariant> basic;
	std::map<std::string, TextureParameter> texture;

	void set(const std::string &key, const ParamVariant &param) { basic.insert_or_assign(key, (param)); }
	void set(const std::string &key, ParamVariant &&param) { basic.insert_or_assign(key, std::move(param)); }
	void set(const std::string &key, const TextureParameter &param) { texture.insert_or_assign(key, (param)); }

	template <typename THash> void hashStatic(THash &hash) const {
		for (auto &pair : texture) {
			hash(pair.first);
			hash(pair.second.defaultTexcoordBinding);
		}
	}
};

struct Material {
public:
	std::vector<FeaturePtr> features;
	MaterialParameters parameters;

	template <typename THash> void hashStatic(THash &hash) const {
		for (auto &feature : features) {
			hash(feature.get());
		}
		hash(parameters);
	}
};

} // namespace gfx

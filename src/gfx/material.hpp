#pragma once

#include "params.hpp"
#include <map>
#include <memory>

namespace gfx {

// Container for shader parameters (basic/texture)
struct MaterialParameters {
  std::map<std::string, ParamVariant> basic;

  void set(const std::string &key, const ParamVariant &param) { basic.insert_or_assign(key, (param)); }
  void set(const std::string &key, ParamVariant &&param) { basic.insert_or_assign(key, std::move(param)); }
};

struct Feature;
typedef std::shared_ptr<Feature> FeaturePtr;
struct Material {
public:
  std::vector<FeaturePtr> features;
  MaterialParameters parameters;

  template <typename THash> void hashStatic(THash &hash) const {
    for (auto &feature : features) {
      hash(feature.get());
    }
  }
};

} // namespace gfx

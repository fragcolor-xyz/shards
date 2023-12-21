#ifndef GFX_MATERIAL
#define GFX_MATERIAL

#include "fwd.hpp"
#include "params.hpp"
#include "unique_id.hpp"
#include <map>
#include <memory>
#include <vector>
#include <string_view>

namespace gfx {

namespace detail {
struct PipelineHashCollector;
}

// Container for shader parameters (basic/texture)
/// <div rustbindgen opaque>
struct MaterialParameters {
  std::map<FastString, NumParameter> basic;
  std::map<FastString, TextureParameter> textures;

  void set(FastString key, const NumParameter &param) { basic.insert_or_assign(key, (param)); }
  void set(FastString key, NumParameter &&param) { basic.insert_or_assign(key, std::move(param)); }
  void set(FastString key, const TextureParameter &param) { textures.insert_or_assign(key, (param)); }

  bool setIfChanged(FastString key, NumParameter param) {
    auto it = basic.find(key);
    if (it != basic.end()) {
      if (it->second != param) {
        it->second = param;
        return true;
      }
    } else {
      basic.insert_or_assign(key, param);
      return true;
    }
    return false;
  }
  bool setIfChanged(FastString key, TextureParameter param) {
    auto it = textures.find(key);
    if (it != textures.end()) {
      if (it->second != param) {
        it->second = param;
        return true;
      }
    } else {
      textures.insert_or_assign(key, param);
      return true;
    }
    return false;
  }

  void clear() {
    basic.clear();
    textures.clear();
  }

  template <typename THash> void getPipelineHash(THash &hash) const {
    for (auto &pair : textures) {
      hash(pair.first);
      hash(pair.second.defaultTexcoordBinding);
    }
  }

  void pipelineHashCollect(detail::PipelineHashCollector &pipelineHashCollector) const;
};

/// <div rustbindgen opaque>
struct Material {
public:
  UniqueId id = getNextId();
  friend struct gfx::UpdateUniqueId<Material>;

  std::vector<FeaturePtr> features;
  MaterialParameters parameters;

  template <typename THash> void getPipelineHash(THash &hash) const {
    for (auto &feature : features) {
      hash(feature.get());
    }
    hash(parameters);
  }

  void pipelineHashCollect(detail::PipelineHashCollector &pipelineHashCollector) const;

  UniqueId getId() const { return id; }
  MaterialPtr clone() const { return cloneSelfWithId(this, getNextId()); }

private:
  static UniqueId getNextId();
};

} // namespace gfx

#endif // GFX_MATERIAL

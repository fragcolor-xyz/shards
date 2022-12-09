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

struct PipelineHashCollector;

// Container for shader parameters (basic/texture)
/// <div rustbindgen opaque>
struct MaterialParameters {
  std::map<std::string, ParamVariant> basic;
  std::map<std::string, TextureParameter> texture;

  void set(const std::string_view &key, const ParamVariant &param) { basic.insert_or_assign(std::string(key), (param)); }
  void set(const std::string_view &key, ParamVariant &&param) { basic.insert_or_assign(std::string(key), std::move(param)); }
  void set(const std::string_view &key, const TextureParameter &param) { texture.insert_or_assign(std::string(key), (param)); }

  template <typename THash> void getPipelineHash(THash &hash) const {
    for (auto &pair : texture) {
      hash(pair.first);
      hash(pair.second.defaultTexcoordBinding);
    }
  }

  void pipelineHashCollect(PipelineHashCollector &PipelineHashCollector) const;
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

  void pipelineHashCollect(PipelineHashCollector &PipelineHashCollector) const;

  UniqueId getId() const { return id; }
  MaterialPtr clone() const { return cloneSelfWithId(this, getNextId()); }

private:
  static UniqueId getNextId();
};

} // namespace gfx

#endif // GFX_MATERIAL

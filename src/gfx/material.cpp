#include "material.hpp"
#include "renderer_cache.hpp"
#include "feature.hpp"
#include "texture.hpp"

namespace gfx {
UniqueId Material::getNextId() {
  static UniqueIdGenerator gen(UniqueIdTag::Material);
  return gen.getNext();
}

template <typename H> struct HashStaticApplier<ParamVariant, H> {
  static constexpr bool applies() { return true; }
  static void apply(const ParamVariant &val, H &hasher) {
    std::visit([&](auto &&arg) { hasher(arg); }, val);
  }
};

void MaterialParameters::staticHashCollect(HashCollector &hashCollector) const {
  for (auto &pair : texture) {
    hashCollector(pair.first);
    if (pair.second.texture) {
      hashCollector(1u);
      hashCollector(pair.second.defaultTexcoordBinding);
    } else {
      hashCollector(0u);
    }
  }
}

void Material::staticHashCollect(HashCollector &hashCollector) const {
  for (auto &feature : features) {
    hashCollector(feature->getId());
    hashCollector.addReference(feature);
  }
  parameters.staticHashCollect(hashCollector);
}
} // namespace gfx

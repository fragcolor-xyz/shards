#include "material.hpp"
#include "renderer_cache.hpp"
#include "feature.hpp"
#include "texture.hpp"

namespace gfx {
UniqueId Material::getNextId() {
  static UniqueIdGenerator gen(UniqueIdTag::Material);
  return gen.getNext();
}

template <typename H> struct PipelineHash<ParamVariant, H> {
  static constexpr bool applies() { return true; }
  static void apply(const ParamVariant &val, H &hasher) {
    std::visit([&](auto &&arg) { hasher(arg); }, val);
  }
};

void MaterialParameters::pipelineHashCollect(PipelineHashCollector &PipelineHashCollector) const {
  for (auto &pair : texture) {
    PipelineHashCollector(pair.first);
    if (pair.second.texture) {
      PipelineHashCollector(1u);
      PipelineHashCollector(pair.second.defaultTexcoordBinding);
    } else {
      PipelineHashCollector(0u);
    }
  }
}

void Material::pipelineHashCollect(PipelineHashCollector &PipelineHashCollector) const {
  for (auto &feature : features) {
    PipelineHashCollector(feature->getId());
    PipelineHashCollector.addReference(feature);
  }
  parameters.pipelineHashCollect(PipelineHashCollector);
}
} // namespace gfx

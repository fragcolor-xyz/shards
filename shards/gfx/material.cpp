#include "material.hpp"
#include "pipeline_hash_collector.hpp"
#include "feature.hpp"
#include "texture.hpp"

namespace gfx {
UniqueId Material::getNextId() {
  static UniqueIdGenerator gen(UniqueIdTag::Material);
  return gen.getNext();
}

template <typename H> struct detail::PipelineHash<ParamVariant, H> {
  static constexpr bool applies() { return true; }
  static void apply(const ParamVariant &val, H &hasher) {
    std::visit([&](auto &&arg) { hasher(arg); }, val);
  }
};
} // namespace gfx

#include "material.hpp"
#include "pipeline_hash_collector.hpp"
#include "feature.hpp"
#include "texture.hpp"

namespace gfx {
UniqueId Material::getNextId() {
  static UniqueIdGenerator gen(UniqueIdTag::Material);
  return gen.getNext();
}

template <typename H> struct detail::PipelineHash<NumParameter, H> {
  static void apply(const NumParameter &val, H &hasher) {
    std::visit([&](auto &&arg) { hasher(arg); }, val);
  }
};
} // namespace gfx

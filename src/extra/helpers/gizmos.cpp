#include "helpers.hpp"
#include <gfx/helpers/translation_gizmo.hpp>
#include "linalg_shim.hpp"

namespace shards {
namespace Helpers {

struct TranslationGizmo : public BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::Float4x4Types; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Types; }
  static SHOptionalString help() { return SHCCSTR("Shows a translation gizmo "); }

  gfx::gizmos::TranslationGizmo _gizmo{};

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &helperContext = getContext();

    _gizmo.transform = (shards::Mat4)input;
    helperContext.gizmoContext.updateGizmo(_gizmo);
    return reinterpret_cast<Mat4 &>(_gizmo.transform);
  }

  void cleanup() {}

  SHTypeInfo compose(const SHInstanceData &data) {
    composeCheckMainThread(data);
    composeCheckContext(data);
    return outputTypes().elements[0];
  }
};
void registerGizmoShards() { REGISTER_SHARD("Helpers.TranslationGizmo", TranslationGizmo); }
} // namespace Helpers
} // namespace shards
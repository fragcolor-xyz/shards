#include "helpers.hpp"
#include "../gfx/shards_types.hpp"
#include <linalg_shim.hpp>
#include <params.hpp>
#include <gfx/helpers/translation_gizmo.hpp>
#include <stdexcept>

namespace shards {
namespace Helpers {
using linalg::aliases::float4x4;

struct TranslationGizmo : public BaseConsumer {
  static SHTypesInfo inputTypes() {
    static Types inputTypes = {{CoreInfo::Float4x4Type, gfx::Types::Drawable, gfx::Types::DrawableHierarchy}};
    return inputTypes;
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString help() { return SHCCSTR("Shows a translation gizmo "); }

  PARAM_VAR(_scale, "Scale", "Gizmo scale", {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(TranslationGizmo, PARAM_IMPL_FOR(_scale));

  gfx::gizmos::TranslationGizmo _gizmo{};

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &helperContext = getContext();

    float4x4 inputMat;
    std::function<void(float4x4 & mat)> applyOutputMat;

    switch (input.valueType) {
    case SHType::Seq:
      inputMat = (shards::Mat4)input;
      break;
    case SHType::Object: {
      Type objectType = Type::Object(input.payload.objectVendorId, input.payload.objectTypeId);
      if (objectType == gfx::Types::Drawable) {
        gfx::SHDrawable *drawable = reinterpret_cast<gfx::SHDrawable *>(input.payload.objectValue);
        if (drawable->transformVar.isVariable()) {
          inputMat = (shards::Mat4)drawable->transformVar.get();
          applyOutputMat = [&](float4x4 &outMat) { cloneVar(drawable->transformVar.get(), (shards::Mat4)outMat); };
        } else {
          inputMat = (shards::Mat4)drawable->drawable->transform;
          applyOutputMat = [&](float4x4 &outMat) { drawable->drawable->transform = outMat; };
        }
      } else if (objectType == gfx::Types::DrawableHierarchy) {
        gfx::SHDrawableHierarchy *drawable = reinterpret_cast<gfx::SHDrawableHierarchy *>(input.payload.objectValue);
        if (drawable->transformVar.isVariable()) {
          inputMat = (shards::Mat4)drawable->transformVar.get();
          applyOutputMat = [&](float4x4 &outMat) { cloneVar(drawable->transformVar.get(), (shards::Mat4)outMat); };
        } else {
          inputMat = (shards::Mat4)drawable->drawableHierarchy->transform;
          applyOutputMat = [&](float4x4 &outMat) { drawable->drawableHierarchy->transform = outMat; };
        }
      } else {
        throw std::invalid_argument("input type");
      }
      break;
    }
    default:
      throw std::invalid_argument("input type");
      break;
    }

    _gizmo.transform = inputMat;
    helperContext.gizmoContext.updateGizmo(_gizmo);
    if (applyOutputMat)
      applyOutputMat(_gizmo.transform);

    return reinterpret_cast<Mat4 &>(_gizmo.transform);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseConsumerWarmup(context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    baseConsumerCleanup();
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    composeCheckMainThread(data);
    composeCheckContext(data);
    return outputTypes().elements[0];
  }
};
void registerGizmoShards() { REGISTER_SHARD("Helpers.TranslationGizmo", TranslationGizmo); }
} // namespace Helpers
} // namespace shards
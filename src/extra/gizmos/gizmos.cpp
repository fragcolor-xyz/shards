#include "context.hpp"
#include "../gfx/shards_types.hpp"
#include "gfx/drawables/mesh_tree_drawable.hpp"
#include <linalg_shim.hpp>
#include <params.hpp>
#include <gfx/gizmos/translation_gizmo.hpp>
#include <gfx/gizmos/scaling_gizmo.hpp>
#include <stdexcept>
#include <type_traits>

namespace shards {
namespace Gizmos {
using linalg::aliases::float3;
using linalg::aliases::float4x4;

struct TranslationGizmo : public Base {
  static SHTypesInfo inputTypes() {
    static Types inputTypes = {{CoreInfo::Float4x4Type, gfx::Types::Drawable}};
    return inputTypes;
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString help() { return SHCCSTR("Shows a translation gizmo "); }

  PARAM_VAR(_scale, "Scale", "Gizmo scale", {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(TranslationGizmo, PARAM_IMPL_FOR(_scale));

  gfx::gizmos::TranslationGizmo _gizmo{};

  SHVar activate(SHContext *shContext, const SHVar &input) {
    float4x4 inputMat;
    std::function<void(float4x4 & mat)> applyOutputMat;

    switch (input.valueType) {
    case SHType::Seq:
      inputMat = (shards::Mat4)input;
      break;
    case SHType::Object: {
      gfx::SHDrawable *drawable = reinterpret_cast<gfx::SHDrawable *>(input.payload.objectValue);
      std::visit(
          [&](auto &drawable) {
            using T = std::decay_t<decltype(drawable)>;
            if constexpr (std::is_same_v<T, gfx::MeshTreeDrawable::Ptr>) {
              inputMat = drawable->trs.getMatrix();
              applyOutputMat = [&](float4x4 &outMat) { drawable->trs = outMat; };
            } else {
              inputMat = drawable->transform;
              applyOutputMat = [&](float4x4 &outMat) { drawable->transform = outMat; };
            }
          },
          drawable->drawable);
      break;
    }
    default:
      throw std::invalid_argument("input type");
      break;
    }

    _gizmo.transform = inputMat;

    // Scale based on screen distance
    float3 gizmoLocation = gfx::extractTranslation(_gizmo.transform);
    _gizmo.scale = _gizmoContext->gfxGizmoContext.renderer.getSize(gizmoLocation) * 0.3f;

    _gizmoContext->gfxGizmoContext.updateGizmo(_gizmo);

    if (applyOutputMat)
      applyOutputMat(_gizmo.transform);

    return reinterpret_cast<Mat4 &>(_gizmo.transform);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    baseCleanup();
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);
    return outputTypes().elements[0];
  }
};
struct ScalingGizmo : public Base {
  static SHTypesInfo inputTypes() {
    static Types inputTypes = {{CoreInfo::Float4x4Type, gfx::Types::Drawable}};
    return inputTypes;
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString help() { return SHCCSTR("Shows a scaling gizmo "); }

  PARAM_VAR(_scale, "Scale", "Gizmo scale", {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(ScalingGizmo, PARAM_IMPL_FOR(_scale));

  gfx::gizmos::ScalingGizmo _gizmo{};

  SHVar activate(SHContext *shContext, const SHVar &input) {
    float4x4 inputMat;
    std::function<void(float4x4 & mat)> applyOutputMat;

    switch (input.valueType) {
    case SHType::Seq:
      inputMat = (shards::Mat4)input;
      break;
    case SHType::Object: {
      gfx::SHDrawable *drawable = reinterpret_cast<gfx::SHDrawable *>(input.payload.objectValue);
      std::visit(
          [&](auto &drawable) {
            using T = std::decay_t<decltype(drawable)>;
            if constexpr (std::is_same_v<T, gfx::MeshTreeDrawable::Ptr>) {
              inputMat = drawable->trs.getMatrix();
              applyOutputMat = [&](float4x4 &outMat) { drawable->trs = outMat; };
            } else {
              inputMat = drawable->transform;
              applyOutputMat = [&](float4x4 &outMat) { drawable->transform = outMat; };
            }
          },
          drawable->drawable);
      break;
    }
    default:
      throw std::invalid_argument("input type");
      break;
    }

    _gizmo.transform = inputMat;

    // Scale based on screen distance
    float3 gizmoLocation = gfx::extractTranslation(_gizmo.transform);
    _gizmo.scale = _gizmoContext->gfxGizmoContext.renderer.getSize(gizmoLocation) * 0.3f;

    _gizmoContext->gfxGizmoContext.updateGizmo(_gizmo);

    if (applyOutputMat)
      applyOutputMat(_gizmo.transform);

    return reinterpret_cast<Mat4 &>(_gizmo.transform);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    baseCleanup();
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);
    return outputTypes().elements[0];
  }
};
void registerTranslationGizmoShards() { REGISTER_SHARD("Gizmos.Translation", TranslationGizmo); }
void registerScalingGizmoShards() { REGISTER_SHARD("Gizmos.Scaling", ScalingGizmo); }
} // namespace Gizmos
} // namespace shards

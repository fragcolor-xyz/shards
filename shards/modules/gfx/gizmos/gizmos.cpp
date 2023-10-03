#include "context.hpp"
#include "../shards_types.hpp"
#include "gfx/drawables/mesh_tree_drawable.hpp"
#include <shards/linalg_shim.hpp>
#include <shards/core/params.hpp>
#include <stdexcept>
#include <type_traits>

#define GIZMO_DEBUG 0
#include <gfx/gizmos/translation_gizmo.hpp>
#include <gfx/gizmos/rotation_gizmo.hpp>
#include <gfx/gizmos/scaling_gizmo.hpp>

namespace shards {
namespace Gizmos {
using linalg::aliases::float3;
using linalg::aliases::float4x4;

inline float getScreenSize(ParamVar &v) {
  auto &screenSizeVar = (Var &)v.get();
  return !screenSizeVar.isNone() ? float(screenSizeVar) : 60.0f;
}

struct TranslationGizmo : public Base {
  static SHTypesInfo inputTypes() {
    static Types inputTypes = {{CoreInfo::Float4x4Type, gfx::Types::Drawable}};
    return inputTypes;
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString help() { return SHCCSTR("Shows a translation gizmo"); }

  PARAM_PARAMVAR(_screenSize, "ScreenSize", "Size of the gizmo on screen (UI size)",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_screenSize));

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
    _gizmo.scale = _gizmoContext->gfxGizmoContext.renderer.getConstantScreenSize(gizmoLocation, getScreenSize(_screenSize));

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

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
};

struct RotationGizmo : public Base {
  static SHTypesInfo inputTypes() {
    static Types inputTypes = {{CoreInfo::Float4x4Type, gfx::Types::Drawable}};
    return inputTypes;
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString help() { return SHCCSTR("Shows a rotation gizmo"); }

  // declare parameter named scale
  PARAM_PARAMVAR(_screenSize, "ScreenSize", "Size of the gizmo on screen (UI size)",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_screenSize));

  // gizmo from translation_gizmo.hpp, not this file
  gfx::gizmos::RotationGizmo _gizmo{};

  SHVar activate(SHContext *shContext, const SHVar &input) {
    float4x4 inputMat;
    std::function<void(float4x4 & mat)> applyOutputMat;

    switch (input.valueType) {
    // if sequence given (transform matrix)
    case SHType::Seq:
      inputMat = (shards::Mat4)input;
      break;
    // if object given (drawable)
    case SHType::Object: {
      gfx::SHDrawable *drawable = reinterpret_cast<gfx::SHDrawable *>(input.payload.objectValue);
      std::visit(
          [&](auto &drawable) {
            // let T be the type of the drawable
            using T = std::decay_t<decltype(drawable)>;
            // if same type as MeshTreeDrawable
            if constexpr (std::is_same_v<T, gfx::MeshTreeDrawable::Ptr>) {
              // get the transform matrix from drawable, and set applyOutputMat to a function that sets the transform
              inputMat = drawable->trs.getMatrix();
              applyOutputMat = [&](float4x4 &outMat) { drawable->trs = outMat; };
            } else { // not sure what type this is expecting, just a type with transform member variable?
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
    _gizmo.scale = _gizmoContext->gfxGizmoContext.renderer.getConstantScreenSize(gizmoLocation, getScreenSize(_screenSize));

    _gizmoContext->gfxGizmoContext.updateGizmo(_gizmo);

    // if valid applyOutputMat function, apply the new transform calculated by _gizmo to the drawable
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

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
};

struct ScalingGizmo : public Base {
  static SHTypesInfo inputTypes() {
    static Types inputTypes = {{CoreInfo::Float4x4Type, gfx::Types::Drawable}};
    return inputTypes;
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString help() { return SHCCSTR("Shows a scaling gizmo"); }

  PARAM_PARAMVAR(_screenSize, "ScreenSize", "Size of the gizmo on screen (UI size)",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_screenSize));

  gfx::gizmos::ScalingGizmo _gizmo{};

  SHVar activate(SHContext *shContext, const SHVar &input) {
    float4x4 inputMat;
    std::function<void(float4x4 & mat)> applyOutputMat;

    switch (input.valueType) {
    // if sequence given (transform matrix)
    case SHType::Seq:
      inputMat = (shards::Mat4)input;
      break;
    // if object given (drawable)
    case SHType::Object: {
      gfx::SHDrawable *drawable = reinterpret_cast<gfx::SHDrawable *>(input.payload.objectValue);
      std::visit(
          [&](auto &drawable) {
            // let T be the type of the drawable
            using T = std::decay_t<decltype(drawable)>;
            // if same type as MeshTreeDrawable
            if constexpr (std::is_same_v<T, gfx::MeshTreeDrawable::Ptr>) {
              // get the transform matrix from drawable, and set applyOutputMat to a function that sets the transform
              inputMat = drawable->trs.getMatrix();
              applyOutputMat = [&](float4x4 &outMat) { drawable->trs = outMat; };
            } else { // not sure what type this is expecting, just a type with transform member variable?
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
    _gizmo.scale = _gizmoContext->gfxGizmoContext.renderer.getConstantScreenSize(gizmoLocation, getScreenSize(_screenSize));

    _gizmoContext->gfxGizmoContext.updateGizmo(_gizmo);

    // if valid applyOutputMat function, apply the new transform calculated by _gizmo to the drawable
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

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
};

void registerGizmoShards() {
  REGISTER_SHARD("Gizmos.Translation", TranslationGizmo);
  REGISTER_SHARD("Gizmos.Rotation", RotationGizmo);
  REGISTER_SHARD("Gizmos.Scaling", ScalingGizmo);
}
} // namespace Gizmos
} // namespace shards

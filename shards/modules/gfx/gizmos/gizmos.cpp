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
using namespace linalg::aliases;

inline float getScreenSize(ParamVar &v) {
  auto &screenSizeVar = (Var &)v.get();
  return !screenSizeVar.isNone() ? float(screenSizeVar) : 60.0f;
}

struct TranslationGizmo : public Base {
  static SHTypesInfo inputTypes() {
    static Types inputTypes = {{CoreInfo::Float4x4Type, gfx::ShardsTypes::Drawable}};
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

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
};

struct RotationGizmo : public Base {
  static SHTypesInfo inputTypes() {
    static Types inputTypes = {{CoreInfo::Float4x4Type, gfx::ShardsTypes::Drawable}};
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

    if (applyOutputMat)
      applyOutputMat(_gizmo.transform);

    return reinterpret_cast<Mat4 &>(_gizmo.transform);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
};

struct ScalingGizmo : public Base {
  static SHTypesInfo inputTypes() {
    static Types inputTypes = {{CoreInfo::Float4x4Type, gfx::ShardsTypes::Drawable}};
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

    if (applyOutputMat)
      applyOutputMat(_gizmo.transform);

    return reinterpret_cast<Mat4 &>(_gizmo.transform);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
};

struct Arrow : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::Float4x4Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Shows an arrow"); }

  PARAM_IMPL();

  SHVar activate(SHContext *shContext, const SHVar &input) {
    float4x4 mat = toFloat4x4(input);
    float3 pos = gfx::extractTranslation(mat);
    float3x3 rot = gfx::extractRotationMatrix(mat);
    float4 bodyColor(1.0f, 1.0f, 1.0f, 1.0f);
    float4 capColor(1.0f, 1.0f, 1.0f, 1.0f);
    _gizmoContext->gfxGizmoContext.renderer.addHandle(pos, rot[2], 0.2f, 1.0f, bodyColor, gfx::GizmoRenderer::CapType::Arrow,
                                                      capColor);
    return SHVar{};
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    Base::baseCompose();
    return outputTypes().elements[0];
  }
};

struct Debug : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Shows the renderer debug visuals"); }

  gfx::RequiredGraphicsContext _gfxContext;
  PARAM_IMPL();

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _gfxContext->renderer->processDebugVisuals(_gizmoContext->gfxGizmoContext.renderer.getShapeRenderer());
    return input;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _gfxContext.warmup(context);
    baseWarmup(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _gfxContext.cleanup(context);
    baseCleanup(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    Base::baseCompose();
    _gfxContext.compose(data, _requiredVariables);
    return data.inputType;
  }
};

void registerGizmoShards() {
  REGISTER_SHARD("Gizmos.Translation", TranslationGizmo);
  REGISTER_SHARD("Gizmos.Rotation", RotationGizmo);
  REGISTER_SHARD("Gizmos.Scaling", ScalingGizmo);
  REGISTER_SHARD("Gizmos.Arrow", Arrow);
  REGISTER_SHARD("Gizmos.Debug", Debug);
}
} // namespace Gizmos
} // namespace shards

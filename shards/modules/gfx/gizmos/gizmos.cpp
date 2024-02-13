#include "context.hpp"
#include "../shards_types.hpp"
#include "trs.hpp"
#include "gfx/drawables/mesh_tree_drawable.hpp"
#include <shards/linalg_shim.hpp>
#include <shards/core/params.hpp>
#include <shards/modules/core/linalg.hpp>
#include <stdexcept>
#include <type_traits>

#define GIZMO_DEBUG 0
#include <gfx/gizmos/translation_gizmo.hpp>
#include <gfx/gizmos/rotation_gizmo.hpp>
#include <gfx/gizmos/scaling_gizmo.hpp>

namespace shards {
namespace Gizmos {
using namespace linalg::aliases;
using gfx::TRS;

struct Types {
  static inline shards::Type TRS = Math::LinAlg::Types::TRS;
  static inline shards::Types GizmoInOutTypes = {Types::TRS, Type::SeqOf(Types::TRS)};
};

inline float getScreenSize(ParamVar &v) {
  auto &screenSizeVar = (Var &)v.get();
  return !screenSizeVar.isNone() ? float(screenSizeVar) : 60.0f;
}

inline TRS computePivot(boost::span<gfx::TRSRef> transforms, bool localSpace = false) {
  if (transforms.empty()) {
    throw std::runtime_error("No transforms to compute pivot from");
  }

  if (localSpace) {
    auto &trs = transforms[0];
    return TRS(trs.translation, trs.rotation);
  } else {
    float3 p{};
    for (auto &trs : transforms) {
      p += trs.translation;
    }
    return TRS(p / transforms.size());
  }
}

inline void applyPivotTranslation(boost::span<gfx::TRSRef> transforms, const TRS &oldTRS, const TRS &newTRS) {
  float4x4 transformToApply = linalg::translation_matrix(newTRS.translation - oldTRS.translation);
  for (auto &trs : transforms) {
    float4x4 m = trs.getMatrix();
    trs = linalg::mul(transformToApply, m);
  }
}

template <typename T>
concept Gizmo = requires(T gizmo) {
  { gizmo.pivot } -> std::convertible_to<TRS>;
  { gizmo.transforms.push_back(TRS()) };
};

struct GizmoIO {
  bool _isSeq{};
  boost::container::small_vector<gfx::TRSRef, 16> _trsRefs;

  SeqVar _outVars;
  boost::container::small_vector<gfx::TRSRef, 16> _outTrsRefs;

  TRS pivot;

  SHTypeInfo compose(const SHInstanceData &data) {
    _isSeq = data.inputType.basicType == SHType::Seq;
    if (_isSeq)
      return Types::GizmoInOutTypes._types[1];
    else
      return Types::GizmoInOutTypes._types[0];
  }

  void fillInputTransforms(const SHVar &input) {
    _trsRefs.clear();
    if (_isSeq) {
      for (auto &r : input.payload.seqValue) {
        _trsRefs.push_back(gfx::toTrsRef(r));
      }
    } else {
      _trsRefs.push_back(gfx::toTrsRef(input));
    }
  }
  void fillInput(const SHVar &input) {
    fillInputTransforms(input);
    pivot = computePivot(_trsRefs);
  }

  SHVar fillOutput(boost::span<TRS> modifiedTransforms) {
    _outVars.resize(_trsRefs.size());
    _outTrsRefs.clear();
    for (size_t i = 0; i < _trsRefs.size(); i++) {
      auto &v = _outVars[i];
      if (v.valueType != SHType::Table) {
        (OwnedVar &)v = TableVar();
      }
      auto &tv = (TableVar &)v;
      toVar(tv, modifiedTransforms[i]);
      _outTrsRefs.push_back(gfx::toTrsRef(v));
    }
    if (_isSeq) {
      return _outVars;
    } else {
      return _outVars[0];
    }
  }

  template <typename G>
  void setGizmoInputs(G &gizmo)
    requires Gizmo<G>
  {
    gizmo.pivot = pivot;
    gizmo.transforms.clear();
    for (auto &trs : _trsRefs) {
      gizmo.transforms.push_back(trs);
    }
  }
};

struct TranslationGizmo : public Base {
  static SHTypesInfo inputTypes() { return Types::GizmoInOutTypes; }
  static SHTypesInfo outputTypes() { return Types::GizmoInOutTypes; }
  static SHOptionalString help() { return SHCCSTR("Shows a translation gizmo"); }

  PARAM_PARAMVAR(_screenSize, "ScreenSize", "Size of the gizmo on screen (UI size)",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_screenSize));

  gfx::gizmos::TranslationGizmo _gizmo{};
  GizmoIO _io{};

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return _io.compose(data);
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _io.fillInput(input);
    _io.setGizmoInputs(_gizmo);

    // Scale based on screen distance
    _gizmo.scale =
        _gizmoContext->gfxGizmoContext.renderer.getConstantScreenSize(_io.pivot.translation, getScreenSize(_screenSize));

    _gizmoContext->gfxGizmoContext.updateGizmo(_gizmo);
    if (!_gizmo.movedTransforms.empty()) {
      return _io.fillOutput(_gizmo.movedTransforms);
    } else {
      return input;
    }
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
  }
};

struct RotationGizmo : public Base {
  static SHTypesInfo inputTypes() { return Types::GizmoInOutTypes; }
  static SHTypesInfo outputTypes() { return Types::GizmoInOutTypes; }
  static SHOptionalString help() { return SHCCSTR("Shows a rotation gizmo"); }

  // declare parameter named scale
  PARAM_PARAMVAR(_screenSize, "ScreenSize", "Size of the gizmo on screen (UI size)",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_screenSize));

  // gizmo from translation_gizmo.hpp, not this file
  gfx::gizmos::RotationGizmo _gizmo{};
  GizmoIO _io{};

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return _io.compose(data);
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _io.fillInput(input);
    _io.setGizmoInputs(_gizmo);

    // Scale based on screen distance
    _gizmo.scale =
        _gizmoContext->gfxGizmoContext.renderer.getConstantScreenSize(_io.pivot.translation, getScreenSize(_screenSize));

    _gizmoContext->gfxGizmoContext.updateGizmo(_gizmo);
    if (!_gizmo.movedTransforms.empty()) {
      return _io.fillOutput(_gizmo.movedTransforms);
    } else {
      return input;
    }
  }
  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
  }
};

struct ScalingGizmo : public Base {
  static SHTypesInfo inputTypes() { return Types::GizmoInOutTypes; }
  static SHTypesInfo outputTypes() { return Types::GizmoInOutTypes; }
  static SHOptionalString help() { return SHCCSTR("Shows a scaling gizmo"); }

  PARAM_PARAMVAR(_screenSize, "ScreenSize", "Size of the gizmo on screen (UI size)",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_screenSize));

  gfx::gizmos::ScalingGizmo _gizmo{};
  GizmoIO _io{};

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return _io.compose(data);
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _io.fillInputTransforms(input);

    // Prefer local pivot
    if (_io._trsRefs.size() == 1) {
      _io.pivot = computePivot(_io._trsRefs, true);
    } else {
      _io.pivot = computePivot(_io._trsRefs);
    }

    _io.setGizmoInputs(_gizmo);

    // Scale based on screen distance
    _gizmo.scale =
        _gizmoContext->gfxGizmoContext.renderer.getConstantScreenSize(_io.pivot.translation, getScreenSize(_screenSize));

    _gizmoContext->gfxGizmoContext.updateGizmo(_gizmo);
    if (!_gizmo.movedTransforms.empty()) {
      return _io.fillOutput(_gizmo.movedTransforms);
    } else {
      return input;
    }
  }
  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
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

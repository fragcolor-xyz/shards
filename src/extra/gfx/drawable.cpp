#include "../gfx.hpp"
#include "drawable_utils.hpp"
#include "extra/gfx/drawable_utils.hpp"
#include "extra/gfx/shards_types.hpp"
#include "foundation.hpp"
#include "gfx/drawables/mesh_drawable.hpp"
#include "gfx/fwd.hpp"
#include "runtime.hpp"
#include "shards.h"
#include "shards.hpp"
#include "shards_utils.hpp"
#include <gfx/drawable.hpp>
#include <gfx/error_utils.hpp>
#include <gfx/material.hpp>
#include <gfx/mesh.hpp>
#include <linalg_shim.hpp>
#include <magic_enum.hpp>
#include <params.hpp>
#include <type_traits>
#include <variant>

using namespace shards;
namespace gfx {
struct DrawableShard {
  static inline Type MeshVarType = Type::VariableOf(Types::Mesh);
  static inline Type TransformVarType = Type::VariableOf(CoreInfo::Float4x4Type);

  PARAM_PARAMVAR(_mesh, "Mesh", "The mesh to use for this drawable.", {MeshVarType});
  PARAM_EXT(ParamVar, _material, Types::MaterialParameterInfo);
  PARAM_EXT(ParamVar, _params, Types::ParamsParameterInfo);
  PARAM_EXT(ParamVar, _features, Types::FeaturesParameterInfo);

  PARAM_IMPL(DrawableShard, PARAM_IMPL_FOR(_mesh), PARAM_IMPL_FOR(_material), PARAM_IMPL_FOR(_params), PARAM_IMPL_FOR(_features));

  static SHOptionalString help() { return SHCCSTR(R"(Drawable help text)"); }
  static SHOptionalString inputHelp() { return SHCCSTR("The drawable's transform"); }

  static SHTypesInfo inputTypes() { return CoreInfo::Float4x4Type; }
  static SHTypesInfo outputTypes() { return Types::Drawable; }

  SHDrawable *_drawable{};

  MeshDrawable::Ptr &getMeshDrawable() { return std::get<MeshDrawable::Ptr>(_drawable->drawable); }

  void releaseReturnVar() {}

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    _drawable = Types::DrawableObjectVar.New();
    _drawable->drawable = std::make_shared<MeshDrawable>();
  }

  void cleanup() {
    PARAM_CLEANUP();

    if (_drawable) {
      Types::DrawableObjectVar.Release(_drawable);
      _drawable = {};
    }
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_mesh.isNone())
      throw formatException("Mesh is required");

    return Types::Drawable;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &meshDrawable = getMeshDrawable();

    meshDrawable->transform = toFloat4x4(input);
    meshDrawable->mesh = *varAsObjectChecked<MeshPtr>(_mesh.get(), Types::Mesh);

    if (!_material.isNone()) {
      meshDrawable->material = varAsObjectChecked<SHMaterial>(_material.get(), Types::Material)->material;
    }

    if (!_params.isNone()) {
      initShaderParams(shContext, _params.get().payload.tableValue, meshDrawable->parameters);
    }

    if (!_features.isNone()) {
      meshDrawable->features.clear();
      applyFeatures(shContext, meshDrawable->features, _features.get());
    }

    return Types::DrawableObjectVar.Get(_drawable);
  }
};

// MainWindow is only required if Queue is not specified
struct DrawShard {
  static inline shards::Types SingleDrawableTypes = shards::Types{Types::Drawable};
  static inline Type DrawableSeqType = Type::SeqOf(SingleDrawableTypes);
  static inline shards::Types DrawableTypes{Types::Drawable, DrawableSeqType};

  PARAM_PARAMVAR(_queue, "Queue", "The queue to add the draw command to (Optional). Uses the default queue if not specified",
                 {CoreInfo::NoneType, Type::VariableOf(Types::DrawQueue)});
  PARAM_IMPL(DrawShard, PARAM_IMPL_FOR(_queue));

  static SHTypesInfo inputTypes() { return DrawableTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  RequiredGraphicsContext _graphicsContext{};

  bool isQueueSet() const { return _queue->valueType != SHType::None; }

  void warmup(SHContext *shContext) {
    PARAM_WARMUP(shContext);

    if (!isQueueSet()) {
      _graphicsContext.warmup(shContext);
    }
  }

  void cleanup() {
    _graphicsContext.cleanup();
    PARAM_CLEANUP();
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (!isQueueSet()) {
      // Use default global queue (check main thread)
      composeCheckGfxThread(data);

      _requiredVariables.push_back(RequiredGraphicsContext::getExposedTypeInfo());
    }

    if (data.inputType.basicType == SHType::Seq) {
      OVERRIDE_ACTIVATE(data, activateSeq);
    } else {
      OVERRIDE_ACTIVATE(data, activateSingle);
    }
    return CoreInfo::NoneType;
  }

  DrawQueue &getDrawQueue() {
    SHVar queueVar = _queue.get();
    if (isQueueSet()) {
      return *varAsObjectChecked<SHDrawQueue>(queueVar, Types::DrawQueue)->queue.get();
    } else {
      return *_graphicsContext->getDrawQueue().get();
    }
  }

  template <typename T> void addDrawableToQueue(T &drawable) { getDrawQueue().add(drawable); }

  SHVar activateSingle(SHContext *shContext, const SHVar &input) {
    assert(input.valueType == SHType::Object);
    std::visit(
        [&](auto &drawable) {
          using T = std::decay_t<decltype(drawable)>;
          if constexpr (!std::is_same_v<T, std::monostate>) {
            addDrawableToQueue(drawable); //
          }
        },
        varAsObjectChecked<SHDrawable>(input, Types::Drawable)->drawable);

    return SHVar{};
  }

  SHVar activateSeq(SHContext *shContext, const SHVar &input) {
    auto &seq = input.payload.seqValue;
    for (size_t i = 0; i < seq.len; i++) {
      (void)activateSingle(shContext, seq.elements[i]);
    }

    return SHVar{};
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { throw ActivationError("Unsupported input type"); }
};

struct DrawQueueShard {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return Types::DrawQueue; }
  static SHOptionalString help() { return SHCCSTR("Creates a new drawable queue to record Draw commands into"); }

  static SHParametersInfo parameters() {
    static Parameters parameters = {};
    return parameters;
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    default:
      return Var::Empty;
    }
  }

  void warmup(SHContext *context) {}

  void cleanup() {}

  SHTypeInfo compose(SHInstanceData &data) { return Types::DrawQueue; }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHDrawQueue *shQueue = Types::DrawQueueObjectVar.New();
    shQueue->queue = std::make_shared<DrawQueue>();
    return Types::DrawQueueObjectVar.Get(shQueue);
  }
};

struct ClearQueueShard {
  static SHTypesInfo inputTypes() { return Types::DrawQueue; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Clears a draw queue"); }

  static SHParametersInfo parameters() {
    static Parameters parameters = {};
    return parameters;
  }

  void setParam(int index, const SHVar &value) {}
  SHVar getParam(int index) { return Var::Empty; }

  void warmup(SHContext *context) {}
  void cleanup() {}

  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHDrawQueue *shQueue = reinterpret_cast<SHDrawQueue *>(input.payload.objectValue);
    shQueue->queue->clear();
    return SHVar{};
  }
};

void registerDrawableShards() {
  REGISTER_SHARD("GFX.Drawable", DrawableShard);
  REGISTER_SHARD("GFX.Draw", DrawShard);
  REGISTER_SHARD("GFX.DrawQueue", DrawQueueShard);
  REGISTER_SHARD("GFX.ClearQueue", ClearQueueShard);
}
} // namespace gfx

#include "gfx.hpp"
#include <shards/common_types.hpp>
#include "drawable_utils.hpp"
#include <shards/core/foundation.hpp>
#include <shards/shards.hpp>
#include "shards_utils.hpp"
#include <gfx/drawable.hpp>
#include <gfx/error_utils.hpp>
#include <gfx/material.hpp>
#include <gfx/mesh.hpp>
#include <magic_enum.hpp>
#include <shards/linalg_shim.hpp>
#include <shards/core/params.hpp>
#include <shards/core/ref_output_pool.hpp>
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

  PARAM_IMPL(PARAM_IMPL_FOR(_mesh), PARAM_IMPL_FOR(_material), PARAM_IMPL_FOR(_params), PARAM_IMPL_FOR(_features));

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
    meshDrawable->mesh = varAsObjectChecked<MeshPtr>(_mesh.get(), Types::Mesh);

    if (!_material.isNone()) {
      meshDrawable->material = varAsObjectChecked<SHMaterial>(_material.get(), Types::Material).material;
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
                 {Type::VariableOf(Types::DrawQueue)});
  PARAM_IMPL(PARAM_IMPL_FOR(_queue));

  static SHTypesInfo inputTypes() { return DrawableTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  DrawShard() {}

  void warmup(SHContext *shContext) { PARAM_WARMUP(shContext); }

  void cleanup() { PARAM_CLEANUP(); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (!_queue.isVariable())
      throw ComposeError("Draw requires a queue");

    if (data.inputType.basicType == SHType::Seq) {
      OVERRIDE_ACTIVATE(data, activateSeq);
    } else {
      OVERRIDE_ACTIVATE(data, activateSingle);
    }

    return CoreInfo::NoneType;
  }

  DrawQueue &getDrawQueue() { return *varAsObjectChecked<SHDrawQueue>(_queue.get(), Types::DrawQueue).queue.get(); }

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
        varAsObjectChecked<SHDrawable>(input, Types::Drawable).drawable);
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

} // namespace gfx

template <> struct ::shards::RefOutputPoolItemTraits<gfx::SHDrawQueue *> {
  gfx::SHDrawQueue *newItem() {
    auto queue = gfx::detail::Container::DrawQueueObjectVar.New();
    queue->queue = std::make_shared<gfx::DrawQueue>();
    return queue;
  }
  void release(gfx::SHDrawQueue *&v) { gfx::Types::DrawQueueObjectVar.Release(v); }
  size_t getRefCount(gfx::SHDrawQueue *&v) { return gfx::Types::DrawQueueObjectVar.GetRefCount(v); }
};

namespace gfx {
struct DrawQueueShard {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return Types::DrawQueue; }
  static SHOptionalString help() { return SHCCSTR("Creates a new drawable queue to record Draw commands into"); }

  PARAM_VAR(_autoClear, "AutoClear", "When enabled, automatically clears the queue after items have been rendered",
            {CoreInfo::NoneType, CoreInfo::BoolType});
  PARAM_VAR(_threaded, "Threaded", "When enabled, output uniuqe queue references to be able to use them with channels",
            {CoreInfo::NoneType, CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_autoClear), PARAM_IMPL_FOR(_threaded));

  std::variant<SHDrawQueue *, RefOutputPool<SHDrawQueue *>> _output;

  DrawQueueShard() {
    _autoClear = Var(true);
    _threaded = Var(false);
  }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if ((bool)*_threaded) {
      _output.emplace<RefOutputPool<SHDrawQueue *>>();
    } else {
      _output.emplace<SHDrawQueue *>();
    }
    return Types::DrawQueue;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    if (SHDrawQueue **_queue = std::get_if<SHDrawQueue *>(&_output)) {
      auto &queue = *_queue;
      assert(!queue);
      queue = Types::DrawQueueObjectVar.New();
      queue->queue = std::make_shared<DrawQueue>();
      queue->queue->setAutoClear(_autoClear->isNone() ? true : (bool)*_autoClear);
    }
  }

  void cleanup() { PARAM_CLEANUP(); }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    if (SHDrawQueue **_queue = std::get_if<SHDrawQueue *>(&_output)) {
      if ((bool)*_autoClear)
        (*_queue)->queue->clear();
      return Types::DrawQueueObjectVar.Get((*_queue));
    } else {
      auto &pool = std::get<RefOutputPool<SHDrawQueue *>>(_output);
      pool.recycle();
      auto queue = pool.newValue(
          [&](SHDrawQueue *&_queue) { _queue->queue->setAutoClear(_autoClear->isNone() ? true : (bool)*_autoClear); });
      queue->queue->clear();
      return Types::DrawQueueObjectVar.Get(queue);
    }
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

struct GetQueueDrawablesShard {
  static inline const Type OutputSeqType = Type::SeqOf(Types::Drawable);

  static SHTypesInfo inputTypes() { return Types::DrawQueue; }
  static SHTypesInfo outputTypes() { return OutputSeqType; }
  static SHOptionalString help() { return SHCCSTR("Retrieves the individual drawables in a draw queue"); }

  SeqVar _outputSeq;
  std::vector<SHDrawable *> _objects;

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
    auto &drawables = shQueue->queue->getDrawables();
    _objects.clear();
    _outputSeq.clear();
    _objects.reserve(drawables.size());
    for (auto &drawable : drawables) {
      SHDrawable *ptr = _objects.emplace_back(Types::DrawableObjectVar.New());
      ptr->assign(drawable->clone());
      _outputSeq.push_back(Types::DrawableObjectVar.Get(ptr));
    }

    return _outputSeq;
  }
};

void registerDrawableShards() {
  REGISTER_SHARD("GFX.Drawable", DrawableShard);
  REGISTER_SHARD("GFX.Draw", DrawShard);
  REGISTER_SHARD("GFX.DrawQueue", DrawQueueShard);
  REGISTER_SHARD("GFX.ClearQueue", ClearQueueShard);
  REGISTER_SHARD("GFX.QueueDrawables", GetQueueDrawablesShard);
}
} // namespace gfx

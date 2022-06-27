#include "../gfx.hpp"
#include "material_utils.hpp"
#include "shards_utils.hpp"
#include <gfx/drawable.hpp>
#include <gfx/error_utils.hpp>
#include <gfx/material.hpp>
#include <gfx/mesh.hpp>
#include <linalg_shim.hpp>
#include <magic_enum.hpp>

using namespace shards;
namespace gfx {

void SHDrawable::updateVariables() {
  if (transformVar.isVariable()) {
    drawable->transform = shards::Mat4(transformVar.get());
  }
  shaderParameters.updateVariables(drawable->parameters);

  if (materialVar->valueType != SHType::None) {
    SHMaterial *shMaterial = (SHMaterial *)materialVar.get().payload.objectValue;
    shMaterial->updateVariables();
  }
}

void SHDrawableHierarchy::updateVariables() {
  if (transformVar.isVariable()) {
    drawableHierarchy->transform = shards::Mat4(transformVar.get());
  }
}

struct DrawableShard {
  static inline Type MeshVarType = Type::VariableOf(Types::Mesh);
  static inline Type TransformVarType = Type::VariableOf(CoreInfo::Float4x4Type);

  static inline std::map<std::string, Type> InputTableTypes = {
      std::make_pair("Transform", CoreInfo::Float4x4Type),
      std::make_pair("Mesh", Types::Mesh),
      std::make_pair("Params", Types::ShaderParamTable),
      std::make_pair("Material", Types::Material),
  };

  static inline Parameters params{
      {"Transform", SHCCSTR("The transform variable to use (Optional)"), {CoreInfo::NoneType, TransformVarType}},
      {"Params",
       SHCCSTR("The params variable to use (Optional)"),
       {CoreInfo::NoneType, Type::TableOf(Types::ShaderParamVarTypes), Type::VariableOf(Type::TableOf(Types::ShaderParamVarTypes))}},
  };

  static SHTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static SHTypesInfo outputTypes() { return Types::Drawable; }
  static SHParametersInfo parameters() { return params; }

  ParamVar _transformVar{};
  ParamVar _paramsVar{};
  SHDrawable *_returnVar{};

  void releaseReturnVar() {
    if (_returnVar)
      Types::DrawableObjectVar.Release(_returnVar);
    _returnVar = {};
  }

  void makeNewReturnVar() {
    releaseReturnVar();
    _returnVar = Types::DrawableObjectVar.New();
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _transformVar = value;
      break;
    case 1:
      _paramsVar = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _transformVar;
    case 1:
      return _paramsVar;
    default:
      return Var::Empty;
    }
  }

  void warmup(SHContext *context) {
    _transformVar.warmup(context);
    _paramsVar.warmup(context);
  }

  void cleanup() {
    _transformVar.cleanup();
    _paramsVar.cleanup();
    releaseReturnVar();
  }

  void validateInputTableType(SHTypeInfo &type) {
    auto &inputTable = type.table;
    size_t inputTableLen = inputTable.keys.len;
    for (size_t i = 0; i < inputTableLen; i++) {
      const char *key = inputTable.keys.elements[i];
      SHTypeInfo &type = inputTable.types.elements[i];

      if (strcmp(key, "Params") == 0) {
        validateShaderParamsType(type);
      } else {
        auto expectedTypeIt = InputTableTypes.find(key);
        if (expectedTypeIt == InputTableTypes.end()) {
          throw ComposeError(fmt::format("Unexpected input table key: {}", key));
        }

        if (expectedTypeIt->second != type) {
          throw ComposeError(fmt::format("Unexpected input type for key: {}", key));
        }
      }
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    validateInputTableType(data.inputType);
    return Types::Drawable;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    const SHTable &inputTable = input.payload.tableValue;

    SHVar meshVar{};
    MeshPtr *meshPtr{};
    if (getFromTable(shContext, inputTable, "Mesh", meshVar)) {
      meshPtr = (MeshPtr *)meshVar.payload.objectValue;
    } else {
      throw formatException("Mesh must be set");
    }

    float4x4 transform = linalg::identity;
    SHVar transformVar{};
    if (getFromTable(shContext, inputTable, "Transform", transformVar)) {
      transform = shards::Mat4(transformVar);
    }

    SHVar materialVar{};
    SHMaterial *shMaterial{};
    if (getFromTable(shContext, inputTable, "Material", materialVar)) {
      shMaterial = (SHMaterial *)materialVar.payload.objectValue;
    }

    makeNewReturnVar();
    _returnVar->drawable = std::make_shared<Drawable>(*meshPtr, transform);

    if (shMaterial) {
      _returnVar->materialVar = Types::MaterialObjectVar.Get(shMaterial);
      _returnVar->materialVar.warmup(shContext);
      _returnVar->drawable->material = shMaterial->material;
    }

    SHVar paramsVar{};
    if (getFromTable(shContext, inputTable, "Params", paramsVar)) {
      initConstantShaderParams(_returnVar->drawable->parameters, paramsVar.payload.tableValue);
    }

    if (_paramsVar->valueType != SHType::None) {
      initReferencedShaderParams(shContext, _returnVar->shaderParameters, _paramsVar.get().payload.tableValue);
    }

    if (_transformVar.isVariable()) {
      _returnVar->transformVar = (SHVar &)_transformVar;
      _returnVar->transformVar.warmup(shContext);
    }

    return Types::DrawableObjectVar.Get(_returnVar);
  }
};

// MainWindow is only required if Queue is not specified
struct DrawShard : public BaseConsumer {
  static inline shards::Types SingleDrawableTypes = shards::Types{Types::Drawable, Types::DrawableHierarchy};
  static inline Type DrawableSeqType = Type::SeqOf(SingleDrawableTypes);
  static inline shards::Types DrawableTypes{Types::Drawable, Types::DrawableHierarchy, DrawableSeqType};

  static SHTypesInfo inputTypes() { return DrawableTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHParametersInfo parameters() {
    static Parameters params{
        {"Queue",
         SHCCSTR("The queue to add the draw command to (Optional). Uses the default queue if not specified"),
         {CoreInfo::NoneType, Type::VariableOf(Types::DrawQueue)}},
    };
    return params;
  }

  ParamVar _queueVar{};

  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo{}; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _queueVar = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _queueVar;
    default:
      return Var::Empty;
    }
  }

  void warmup(SHContext *shContext) {
    baseConsumerWarmup(shContext, false);
    _queueVar.warmup(shContext);
  }

  void cleanup() {
    baseConsumerCleanup();
    _queueVar.cleanup();
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (_queueVar->valueType != SHType::None) {
      // Use the specified queue
    } else {
      // Use default global queue (check main thread)
      composeCheckMainThread(data);
      composeCheckMainWindowGlobals(data);
    }

    if (data.inputType.basicType == SHType::Seq) {
      OVERRIDE_ACTIVATE(data, activateSeq);
    } else {
      OVERRIDE_ACTIVATE(data, activateSingle);
    }
    return CoreInfo::NoneType;
  }

  DrawQueue &getDrawQueue() {
    SHVar queueVar = _queueVar.get();
    if (queueVar.payload.objectValue) {
      return *((SHDrawQueue *)queueVar.payload.objectValue)->queue.get();
    } else {
      return *getMainWindowGlobals().drawQueue.get();
    }
  }

  template <typename T> void addDrawableToQueue(T drawable) { getDrawQueue().add(drawable); }

  SHVar activateSingle(SHContext *shContext, const SHVar &input) {
    assert(input.valueType == SHType::Object);
    if (input.payload.objectTypeId == Types::DrawableTypeId) {
      SHDrawable *shDrawable = (SHDrawable *)input.payload.objectValue;
      assert(shDrawable);
      shDrawable->updateVariables();
      addDrawableToQueue(shDrawable->drawable);
    } else if (input.payload.objectTypeId == Types::DrawableHierarchyTypeId) {
      SHDrawableHierarchy *shDrawableHierarchy = (SHDrawableHierarchy *)input.payload.objectValue;
      assert(shDrawableHierarchy);
      shDrawableHierarchy->updateVariables();
      addDrawableToQueue(shDrawableHierarchy->drawableHierarchy);
    } else {
      throw ActivationError("Unknown drawable object type");
    }

    return SHVar{};
  }

  SHVar activateSeq(SHContext *shContext, const SHVar &input) {
    auto &seq = input.payload.seqValue;
    for (size_t i = 0; i < seq.len; i++) {
      (void)activateSingle(shContext, seq.elements[i]);
    }

    return SHVar{};
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { throw ActivationError("GFX.Draw: Unsupported input type"); }
};

struct DrawQueueShard {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
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
    SHDrawQueue *shQueue = (SHDrawQueue *)input.payload.objectValue;
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

#include "../gfx.hpp"
#include "shards_utils.hpp"
#include "material_utils.hpp"
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
      {"Transform", SHCCSTR("The transform variable to use"), {TransformVarType}},
      {"Params",
       SHCCSTR("The params variable to use"),
       {Type::TableOf(Types::ShaderParamVarTypes), Type::VariableOf(Type::TableOf(Types::ShaderParamVarTypes))}},
  };

  static SHTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static SHTypesInfo outputTypes() { return Types::Drawable; }
  static SHParametersInfo parameters() { return params; }

  ParamVar _transformVar{};
  ParamVar _paramsVar{};

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

    SHDrawable *shDrawable = Types::DrawableObjectVar.New();
    shDrawable->drawable = std::make_shared<Drawable>(*meshPtr, transform);

    if (shMaterial) {
      shDrawable->materialVar = Types::MaterialObjectVar.Get(shMaterial);
      shDrawable->materialVar.warmup(shContext);
      shDrawable->drawable->material = shMaterial->material;
    }

    SHVar paramsVar{};
    if (getFromTable(shContext, inputTable, "Params", paramsVar)) {
      initConstantShaderParams(shDrawable->drawable->parameters, paramsVar.payload.tableValue);
    }

    if (_paramsVar->valueType != SHType::None) {
      initReferencedShaderParams(shContext, shDrawable->shaderParameters, _paramsVar.get().payload.tableValue);
    }

    if (_transformVar.isVariable()) {
      shDrawable->transformVar = (SHVar &)_transformVar;
      shDrawable->transformVar.warmup(shContext);
    }

    return Types::DrawableObjectVar.Get(shDrawable);
  }
};

struct DrawShard : public BaseConsumer {
  static inline Type DrawableSeqType = Type::SeqOf(Types::Drawable);
  static inline shards::Types DrawableTypes{Types::Drawable, DrawableSeqType};

  static SHTypesInfo inputTypes() { return DrawableTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHParametersInfo parameters() { return SHParametersInfo(); }

  void warmup(SHContext *shContext) { baseConsumerWarmup(shContext); }
  void cleanup() { baseConsumerCleanup(); }
  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.inputType.basicType == SHType::Seq) {
      OVERRIDE_ACTIVATE(data, activateSeq);
    } else {
      OVERRIDE_ACTIVATE(data, activateSingle);
    }
    return CoreInfo::NoneType;
  }

  void addDrawableToQueue(DrawablePtr drawable) {
    auto &dq = getMainWindowGlobals().drawQueue;
    dq.add(drawable);
  }

  void updateSHDrawable(SHDrawable *shDrawable) {
    // Update transform if it's referencing a context variable
    if (shDrawable->transformVar.isVariable()) {
      shDrawable->drawable->transform = shards::Mat4(shDrawable->transformVar.get());
    }
  }

  SHVar activateSeq(SHContext *shContext, const SHVar &input) {
    auto &seq = input.payload.seqValue;
    for (size_t i = 0; i < seq.len; i++) {
      SHDrawable *shDrawable = (SHDrawable *)seq.elements[i].payload.objectValue;
      assert(shDrawable);
      shDrawable->updateVariables();
      addDrawableToQueue(shDrawable->drawable);
    }

    return SHVar{};
  }

  SHVar activateSingle(SHContext *shContext, const SHVar &input) {
    SHDrawable *shDrawable = (SHDrawable *)input.payload.objectValue;
    updateSHDrawable(shDrawable);
    addDrawableToQueue(shDrawable->drawable);

    return SHVar{};
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { throw ActivationError("GFX.Draw: Unsupported input type"); }
};

void registerDrawableShards() {
  REGISTER_SHARD("GFX.Drawable", DrawableShard);
  REGISTER_SHARD("GFX.Draw", DrawShard);
}
} // namespace gfx

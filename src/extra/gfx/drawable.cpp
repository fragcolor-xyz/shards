#include "../gfx.hpp"
#include "material_utils.hpp"
#include <gfx/drawable.hpp>
#include <gfx/error_utils.hpp>
#include <gfx/material.hpp>
#include <gfx/mesh.hpp>
#include <linalg_shim.hpp>
#include <magic_enum.hpp>

using namespace chainblocks;
namespace gfx {

void CBDrawable::updateVariables() {
  if (transformVar.isVariable()) {
    drawable->transform = chainblocks::Mat4(transformVar.get());
  }
  shaderParameters.updateVariables(drawable->parameters);

  if (materialVar->valueType != CBType::None) {
    CBMaterial *cbMaterial = (CBMaterial *)materialVar.get().payload.objectValue;
    cbMaterial->updateVariables();
  }
}

struct DrawableBlock {
  static inline Type MeshVarType = Type::VariableOf(MeshType);
  static inline Type TransformVarType = Type::VariableOf(CoreInfo::Float4x4Type);

  static inline std::map<std::string, Type> InputTableTypes = {
      std::make_pair("Transform", CoreInfo::Float4x4Type),
      std::make_pair("Mesh", MeshType),
      std::make_pair("Params", ShaderParamTableType),
      std::make_pair("Material", MaterialType),
  };

  static inline Parameters params{
      {"Transform", CBCCSTR("The transform variable to use"), {TransformVarType}},
      {"Params",
       CBCCSTR("The params variable to use"),
       {Type::TableOf(ShaderParamVarTypes), Type::VariableOf(Type::TableOf(ShaderParamVarTypes))}},
  };

  static CBTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static CBTypesInfo outputTypes() { return DrawableType; }
  static CBParametersInfo parameters() { return params; }

  ParamVar _transformVar{};
  ParamVar _paramsVar{};

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _transformVar;
    case 1:
      return _paramsVar;
    default:
      return Var::Empty;
    }
  }

  void warmup(CBContext *context) {
    _transformVar.warmup(context);
    _paramsVar.warmup(context);
  }

  void cleanup() {
    _transformVar.cleanup();
    _paramsVar.cleanup();
  }

  void validateInputTableType(CBTypeInfo &type) {
    auto &inputTable = type.table;
    size_t inputTableLen = inputTable.keys.len;
    for (size_t i = 0; i < inputTableLen; i++) {
      const char *key = inputTable.keys.elements[i];
      CBTypeInfo &type = inputTable.types.elements[i];

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

  CBTypeInfo compose(CBInstanceData &data) {
    validateInputTableType(data.inputType);
    return DrawableType;
  }

  bool getFromTable(CBContext *cbContext, const CBTable &table, const char *key, CBVar &outVar) {
    if (table.api->tableContains(table, key)) {
      const CBVar *var = table.api->tableAt(table, key);
      if (var->valueType == CBType::ContextVar) {
        CBVar *refencedVariable = referenceVariable(cbContext, var->payload.stringValue);
        outVar = *var;
        releaseVariable(refencedVariable);
      }
      outVar = *var;
      return true;
    }
    return false;
  }

  CBVar activate(CBContext *cbContext, const CBVar &input) {
    const CBTable &inputTable = input.payload.tableValue;

    CBVar meshVar{};
    MeshPtr *meshPtr{};
    if (getFromTable(cbContext, inputTable, "Mesh", meshVar)) {
      meshPtr = (MeshPtr *)meshVar.payload.objectValue;
    } else {
      throw formatException("Mesh must be set");
    }

    float4x4 transform = linalg::identity;
    CBVar transformVar{};
    if (getFromTable(cbContext, inputTable, "Transform", transformVar)) {
      transform = chainblocks::Mat4(transformVar);
    }

    CBVar materialVar{};
    CBMaterial *cbMaterial{};
    if (getFromTable(cbContext, inputTable, "Material", materialVar)) {
      cbMaterial = (CBMaterial *)materialVar.payload.objectValue;
    }

    CBDrawable *cbDrawable = DrawableObjectVar.New();
    cbDrawable->drawable = std::make_shared<Drawable>(*meshPtr, transform);

    if (cbMaterial) {
      cbDrawable->materialVar = MaterialObjectVar.Get(cbMaterial);
      cbDrawable->materialVar.warmup(cbContext);
      cbDrawable->drawable->material = cbMaterial->material;
    }

    CBVar paramsVar{};
    if (getFromTable(cbContext, inputTable, "Params", paramsVar)) {
      initConstantShaderParams(cbDrawable->drawable->parameters, paramsVar.payload.tableValue);
    }

    if (_paramsVar->valueType != CBType::None) {
      initReferencedShaderParams(cbContext, cbDrawable->shaderParameters, _paramsVar.get().payload.tableValue);
    }

    if (_transformVar.isVariable()) {
      cbDrawable->transformVar = (CBVar &)_transformVar;
      cbDrawable->transformVar.warmup(cbContext);
    }

    return DrawableObjectVar.Get(cbDrawable);
  }
};

struct DrawBlock : public BaseConsumer {
  static inline Type DrawableSeqType = Type::SeqOf(DrawableType);
  static inline Types DrawableTypes{DrawableType, DrawableSeqType};

  static CBTypesInfo inputTypes() { return DrawableTypes; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBParametersInfo parameters() { return CBParametersInfo(); }

  void warmup(CBContext *cbContext) { baseConsumerWarmup(cbContext); }
  void cleanup() { baseConsumerCleanup(); }
  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.inputType.basicType == CBType::Seq) {
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

  void updateCBDrawable(CBDrawable *cbDrawable) {
    // Update transform if it's referencing a context variable
    if (cbDrawable->transformVar.isVariable()) {
      cbDrawable->drawable->transform = chainblocks::Mat4(cbDrawable->transformVar.get());
    }
  }

  CBVar activateSeq(CBContext *cbContext, const CBVar &input) {
    auto &seq = input.payload.seqValue;
    for (size_t i = 0; i < seq.len; i++) {
      CBDrawable *cbDrawable = (CBDrawable *)seq.elements[i].payload.objectValue;
      assert(cbDrawable);
      cbDrawable->updateVariables();
      addDrawableToQueue(cbDrawable->drawable);
    }

    return CBVar{};
  }

  CBVar activateSingle(CBContext *cbContext, const CBVar &input) {
    CBDrawable *cbDrawable = (CBDrawable *)input.payload.objectValue;
    updateCBDrawable(cbDrawable);
    addDrawableToQueue(cbDrawable->drawable);

    return CBVar{};
  }

  CBVar activate(CBContext *cbContext, const CBVar &input) { throw ActivationError("GFX.Draw: Unsupported input type"); }
};

void registerDrawableBlocks() {
  REGISTER_CBLOCK("GFX.Drawable", DrawableBlock);
  REGISTER_CBLOCK("GFX.Draw", DrawBlock);
}
} // namespace gfx

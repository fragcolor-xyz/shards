#include "../gfx.hpp"
#include "chainblocks_types.hpp"
#include "chainblocks_utils.hpp"
#include "linalg_shim.hpp"
#include "material_utils.hpp"
#include <gfx/material.hpp>

using namespace chainblocks;
namespace gfx {
void CBShaderParameters::updateVariables(MaterialParameters &output) {
  for (CBBasicShaderParameter &param : basic) {
    ParamVariant variant;
    if (varToParam(param.var.get(), variant)) {
      output.set(param.key, variant);
    }
  }
}

void CBMaterial::updateVariables() { shaderParameters.updateVariables(material->parameters); }

struct MaterialBlock {
  static inline Type MeshVarType = Type::VariableOf(Types::Mesh);
  static inline Type TransformVarType = Type::VariableOf(CoreInfo::Float4x4Type);

  static inline std::map<std::string, Type> InputTableTypes{};

  static inline Parameters params{
      {"Params",
       CBCCSTR("The params variable to use"),
       {Type::TableOf(Types::ShaderParamVarTypes), Type::VariableOf(Type::TableOf(Types::ShaderParamVarTypes))}},
  };

  static CBTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static CBTypesInfo outputTypes() { return Types::Material; }
  static CBParametersInfo parameters() { return params; }

  ParamVar _paramsVar{};

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _paramsVar = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _paramsVar;
    default:
      return Var::Empty;
    }
  }

  void warmup(CBContext *context) { _paramsVar.warmup(context); }

  void cleanup() { _paramsVar.cleanup(); }

  void validateInputTableType(CBTypeInfo &type) {
    auto &inputTable = type.table;
    size_t inputTableLen = inputTable.keys.len;
    for (size_t i = 0; i < inputTableLen; i++) {
      const char *key = inputTable.keys.elements[i];
      CBTypeInfo &type = inputTable.types.elements[i];

      if (strcmp(key, "Params") == 0) {
        validateShaderParamsType(type);
      }
    }
  }

  CBTypeInfo compose(CBInstanceData &data) {
    validateInputTableType(data.inputType);
    return Types::Material;
  }

  CBVar activate(CBContext *cbContext, const CBVar &input) {
    const CBTable &inputTable = input.payload.tableValue;

    CBMaterial *cbMaterial = Types::MaterialObjectVar.New();
    cbMaterial->material = std::make_shared<Material>();

    CBVar paramsVar{};
    if (getFromTable(cbContext, inputTable, "Params", paramsVar)) {
      initConstantShaderParams(cbMaterial->material->parameters, paramsVar.payload.tableValue);
    }

    if (_paramsVar->valueType != CBType::None) {
      initReferencedShaderParams(cbContext, cbMaterial->shaderParameters, _paramsVar.get().payload.tableValue);
    }

    return Types::MaterialObjectVar.Get(cbMaterial);
  }
};

void registerMaterialBlocks() { REGISTER_CBLOCK("GFX.Material", MaterialBlock); }

} // namespace gfx

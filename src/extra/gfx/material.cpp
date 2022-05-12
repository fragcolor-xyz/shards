#include "../gfx.hpp"
#include "shards_types.hpp"
#include "shards_utils.hpp"
#include "linalg_shim.hpp"
#include "material_utils.hpp"
#include <gfx/material.hpp>

using namespace shards;
namespace gfx {
void SHShaderParameters::updateVariables(MaterialParameters &output) {
  for (SHBasicShaderParameter &param : basic) {
    ParamVariant variant;
    varToParam(param.var.get(), variant);
    output.set(param.key, variant);
  }
}

void SHMaterial::updateVariables() { shaderParameters.updateVariables(material->parameters); }

struct MaterialShard {
  static inline Type MeshVarType = Type::VariableOf(Types::Mesh);
  static inline Type TransformVarType = Type::VariableOf(CoreInfo::Float4x4Type);

  static inline std::map<std::string, Type> InputTableTypes{};

  static inline Parameters params{
      {"Params",
       SHCCSTR("The params variable to use"),
       {Type::TableOf(Types::ShaderParamVarTypes), Type::VariableOf(Type::TableOf(Types::ShaderParamVarTypes))}},
  };

  static SHTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static SHTypesInfo outputTypes() { return Types::Material; }
  static SHParametersInfo parameters() { return params; }

  ParamVar _paramsVar{};

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _paramsVar = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _paramsVar;
    default:
      return Var::Empty;
    }
  }

  void warmup(SHContext *context) { _paramsVar.warmup(context); }

  void cleanup() { _paramsVar.cleanup(); }

  void validateInputTableType(SHTypeInfo &type) {
    auto &inputTable = type.table;
    size_t inputTableLen = inputTable.keys.len;
    for (size_t i = 0; i < inputTableLen; i++) {
      const char *key = inputTable.keys.elements[i];
      SHTypeInfo &type = inputTable.types.elements[i];

      if (strcmp(key, "Params") == 0) {
        validateShaderParamsType(type);
      }
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    validateInputTableType(data.inputType);
    return Types::Material;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    const SHTable &inputTable = input.payload.tableValue;

    SHMaterial *shMaterial = Types::MaterialObjectVar.New();
    shMaterial->material = std::make_shared<Material>();

    SHVar paramsVar{};
    if (getFromTable(shContext, inputTable, "Params", paramsVar)) {
      initConstantShaderParams(shMaterial->material->parameters, paramsVar.payload.tableValue);
    }

    if (_paramsVar->valueType != SHType::None) {
      initReferencedShaderParams(shContext, shMaterial->shaderParameters, _paramsVar.get().payload.tableValue);
    }

    return Types::MaterialObjectVar.Get(shMaterial);
  }
};

void registerMaterialShards() { REGISTER_SHARD("GFX.Material", MaterialShard); }

} // namespace gfx

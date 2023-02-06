#include "../gfx.hpp"
#include "shards_types.hpp"
#include "shards_utils.hpp"
#include "linalg_shim.hpp"
#include "drawable_utils.hpp"
#include <gfx/material.hpp>
#include <params.hpp>

using namespace shards;
namespace gfx {
void SHShaderParameters::updateVariables(MaterialParameters &output) {
  for (SHBasicShaderParameter &param : basic) {
    ParamVariant variant = varToParam(param.var.get());
    output.set(param.key, variant);
  }
  for (SHBasicShaderParameter &texture : textures) {
    TexturePtr variant = varToTexture(texture.var.get());
    output.set(texture.key, variant);
  }
}

void SHMaterial::updateVariables() { shaderParameters.updateVariables(material->parameters); }

struct MaterialShard {
  static inline Type MeshVarType = Type::VariableOf(Types::Mesh);
  static inline Type TransformVarType = Type::VariableOf(CoreInfo::Float4x4Type);
  static inline Type TexturesTable = Type::TableOf(Types::TextureTypes);
  static inline Type ShaderParamVarTable = Type::TableOf(Types::ShaderParamVarTypes);

  static inline std::map<std::string, Type> InputTableTypes{};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static SHTypesInfo outputTypes() { return Types::Material; }

  PARAM_PARAMVAR(_paramsVar, "Params", "The params variable to use (Optional)",
                 {CoreInfo::NoneType, ShaderParamVarTable, Type::VariableOf(ShaderParamVarTable)});
  PARAM_PARAMVAR(_texturesVar, "Textures", "The textures variable to use (Optional)",
                 {CoreInfo::NoneType, TexturesTable, Type::VariableOf(TexturesTable)});
  PARAM_IMPL(MaterialShard, PARAM_IMPL_FOR(_paramsVar), PARAM_IMPL_FOR(_texturesVar));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup() { PARAM_CLEANUP(); }

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

    initShaderParams(shContext, inputTable, _paramsVar, _texturesVar, shMaterial->material->parameters,
                     shMaterial->shaderParameters);

    return Types::MaterialObjectVar.Get(shMaterial);
  }
};

void registerMaterialShards() { REGISTER_SHARD("GFX.Material", MaterialShard); }

} // namespace gfx

#ifndef C7AA640C_1748_42BC_8772_0B8F4E136CEA
#define C7AA640C_1748_42BC_8772_0B8F4E136CEA

#include "gfx/error_utils.hpp"
#include "shards_types.hpp"
#include "shards_utils.hpp"
#include <ops_internal.hpp>
#include <foundation.hpp>
#include <gfx/material.hpp>
#include <gfx/params.hpp>
#include <magic_enum.hpp>
#include <shards.hpp>
#include <spdlog/spdlog.h>

namespace gfx {
inline void varToTexture(const SHVar &var, TexturePtr &outVariant) {
  switch (var.valueType) {
  case SHType::Object: {
    shards::Type type = shards::Type::Object(var.payload.objectVendorId, var.payload.objectTypeId);
    if (type == Types::Texture) {
      auto ptr = reinterpret_cast<TexturePtr *>(var.payload.objectValue);
      assert(ptr);
      outVariant = *ptr;
    } else {
      throw formatException("Expected texture object");
    }
  } break;
  default:
    throw formatException("Value type {} can not be converted to TextureVariant", magic_enum::enum_name(var.valueType));
  };
}

inline void varToParam(const SHVar &var, ParamVariant &outVariant) {
  switch (var.valueType) {
  case SHType::Float: {
    float vec = float(var.payload.floatValue);
    outVariant = vec;
  } break;
  case SHType::Float2: {
    float2 vec;
    vec.x = float(var.payload.float2Value[0]);
    vec.y = float(var.payload.float2Value[1]);
    outVariant = vec;
  } break;
  case SHType::Float3: {
    float3 vec;
    memcpy(&vec.x, &var.payload.float3Value, sizeof(float) * 3);
    outVariant = vec;
  } break;
  case SHType::Float4: {
    float4 vec;
    memcpy(&vec.x, &var.payload.float4Value, sizeof(float) * 4);
    outVariant = vec;
  } break;
  case SHType::Seq:
    if (var.innerType == SHType::Float4) {
      float4x4 matrix;
      const SHSeq &seq = var.payload.seqValue;
      for (size_t i = 0; i < std::min<size_t>(4, seq.len); i++) {
        float4 row;
        memcpy(&row.x, &seq.elements[i].payload.float4Value, sizeof(float) * 4);
        matrix[i] = row;
      }
      outVariant = matrix;
    } else {
      throw formatException("Seq inner type {} can not be converted to ParamVariant", magic_enum::enum_name(var.valueType));
    }
    break;
  default:
    throw formatException("Value type {} can not be converted to ParamVariant", magic_enum::enum_name(var.valueType));
  }
}

inline bool tryVarToParam(const SHVar &var, ParamVariant &outVariant) {
  try {
    varToParam(var, outVariant);
    return true;
  } catch (std::exception &e) {
    spdlog::error("{}", e.what());
    return false;
  }
}

inline void initConstantShaderParams(const SHTable &paramsTable, MaterialParameters &out) {
  SHTableIterator it{};
  SHString key{};
  SHVar value{};
  paramsTable.api->tableGetIterator(paramsTable, &it);
  while (paramsTable.api->tableNext(paramsTable, &it, &key, &value)) {
    ParamVariant variant;
    if (tryVarToParam(value, variant)) {
      out.set(key, variant);
    }
  }
}

inline void initConstantTextureParams(const SHTable &texturesTable, MaterialParameters &out) {
  SHTableIterator it{};
  SHString key{};
  SHVar value{};
  texturesTable.api->tableGetIterator(texturesTable, &it);
  while (texturesTable.api->tableNext(texturesTable, &it, &key, &value)) {
    TexturePtr texture;
    varToTexture(value, texture);
    out.set(key, texture);
  }
}

inline void initReferencedShaderParams(SHContext *shContext, const SHTable &inTable,
                                       std::vector<SHBasicShaderParameter> &outParams) {
  SHTableIterator it{};
  SHString key{};
  SHVar value{};
  inTable.api->tableGetIterator(inTable, &it);
  while (inTable.api->tableNext(inTable, &it, &key, &value)) {
    shards::ParamVar paramVar(value);
    paramVar.warmup(shContext);
    outParams.emplace_back(key, std::move(paramVar));
  }
}

inline void initShaderParams(SHContext *shContext, const SHTable &inputTable, const shards::ParamVar &inParams,
                             const shards::ParamVar &inTextures, MaterialParameters &outParams, SHShaderParameters &outSHParams) {
  SHVar paramsVar{};
  if (getFromTable(shContext, inputTable, "Params", paramsVar)) {
    initConstantShaderParams(paramsVar.payload.tableValue, outParams);
  }
  if (inParams->valueType != SHType::None) {
    initReferencedShaderParams(shContext, inParams.get().payload.tableValue, outSHParams.basic);
  }

  SHVar texturesVar{};
  if (getFromTable(shContext, inputTable, "Textures", texturesVar)) {
    initConstantTextureParams(texturesVar.payload.tableValue, outParams);
  }
  if (inTextures->valueType != SHType::None) {
    initReferencedShaderParams(shContext, inTextures.get().payload.tableValue, outSHParams.textures);
  }
}

inline void validateShaderParamsType(const SHTypeInfo &type) {
  using shards::ComposeError;

  if (type.basicType != SHType::Table) {
    throw formatException("Wrong type for Params: {}, should be a table", magic_enum::enum_name(type.basicType));
  }

  size_t tableLen = type.table.types.len;
  for (size_t i = 0; i < tableLen; i++) {
    const char *key = type.table.keys.elements[i];
    auto valueType = type.table.types.elements[i];
    bool matched = false;
    for (auto &supportedType : Types::ShaderParamTypes._types) {
      if (valueType == supportedType) {
        matched = true;
        break;
      }
    }

    if (!matched) {
      throw formatException("Unsupported parameter type for param {}: {}", key, magic_enum::enum_name(valueType.basicType));
    }
  }
}

inline void validateTexturesInputType(const SHTypeInfo &type) {
  if (type.basicType != SHType::Table)
    throw formatException("Textures should be a table");

  auto &tableTypes = type.table.types;
  for (auto &type : tableTypes) {
    if (type != Types::Texture)
      throw formatException("Unexpected type in Textures table");
  }
}

struct TableValidationResult {
  bool isValid{};
  std::string unexpectedKey;
  operator bool() const { return isValid; }
};

inline void validateDrawableInputTableEntry(const char *key, const SHTypeInfo &type) {
  if (strcmp(key, "Params") == 0) {
    validateShaderParamsType(type);
  } else if (strcmp(key, "Textures") == 0) {
    validateTexturesInputType(type);
  } else {
    auto expectedTypeIt = Types::DrawableInputTableTypes.find(key);
    if (expectedTypeIt == Types::DrawableInputTableTypes.end()) {
      throw formatException("Unexpected input table key: {}", key);
    }

    if (expectedTypeIt->second != type) {
      throw formatException("Unexpected input type for key: {}. expected {}, got {}", key, (SHTypeInfo &)expectedTypeIt->second,
                            type);
    }
  }
}

inline void validateDrawableInputTableType(const SHTypeInfo &type) {
  auto &inputTable = type.table;
  size_t inputTableLen = inputTable.keys.len;
  for (size_t i = 0; i < inputTableLen; i++) {
    const char *key = inputTable.keys.elements[i];
    SHTypeInfo &type = inputTable.types.elements[i];

    validateDrawableInputTableEntry(key, type);
  }
}

} // namespace gfx

#endif /* C7AA640C_1748_42BC_8772_0B8F4E136CEA */

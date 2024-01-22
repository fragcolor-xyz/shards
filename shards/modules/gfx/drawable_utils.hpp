#ifndef C7AA640C_1748_42BC_8772_0B8F4E136CEA
#define C7AA640C_1748_42BC_8772_0B8F4E136CEA

#include "gfx/error_utils.hpp"
#include "gfx/shader/types.hpp"
#include <shards/shards.h>
#include "shards_types.hpp"
#include "shards_utils.hpp"
#include <shards/core/ops_internal.hpp>
#include <shards/core/foundation.hpp>
#include <gfx/material.hpp>
#include <gfx/params.hpp>
#include <gfx/texture.hpp>
#include <magic_enum.hpp>
#include <optional>
#include <shards/shards.hpp>
#include <spdlog/spdlog.h>
#include <variant>

namespace gfx {
inline TexturePtr varToTexture(const SHVar &var) {
  TexturePtr result;
  if (var.payload.objectTypeId == ShardsTypes::TextureCubeTypeId) {
    result = varAsObjectChecked<TexturePtr>(var, ShardsTypes::TextureCube);
  } else if (var.payload.objectTypeId == ShardsTypes::TextureTypeId) {
    result = varAsObjectChecked<TexturePtr>(var, ShardsTypes::Texture);
  } else {
    SHInstanceData data{};
    auto varType = shards::deriveTypeInfo(var, data);
    DEFER({ shards::freeDerivedInfo(varType); });
    throw formatException("Invalid texture variable type: {}", varType);
  }

  if (!result) {
    throw formatException("Texture is null");
  }
  return result;
}

using ShaderParamVariant = std::variant<NumParameter, TextureParameter, BufferPtr>;

inline ShaderParamVariant varToShaderParameter(const SHVar &var) {
  auto isTypedSeq = [&](const SHType basicType) {
    if (var.innerType == SHType::Float4)
      return true;
    for (size_t i = 0; i < var.payload.seqValue.len; i++) {
      if (var.payload.seqValue.elements[i].valueType != basicType)
        return false;
    }
    return true;
  };

  switch (var.valueType) {
  case SHType::Float: {
    float vec = float(var.payload.floatValue);
    return vec;
  }
  case SHType::Float2: {
    float2 vec;
    vec.x = float(var.payload.float2Value[0]);
    vec.y = float(var.payload.float2Value[1]);
    return vec;
  }
  case SHType::Float3: {
    float3 vec;
    memcpy(&vec.x, &var.payload.float3Value, sizeof(float) * 3);
    return vec;
  }
  case SHType::Float4: {
    float4 vec;
    memcpy(&vec.x, &var.payload.float4Value, sizeof(float) * 4);
    return vec;
  }
  case SHType::Int: {
    return int32_t(var.payload.intValue);
  }
  case SHType::Int2: {
    int2 vec;
    vec.x = int(var.payload.int2Value[0]);
    vec.y = int(var.payload.int2Value[1]);
    return vec;
  }
  case SHType::Int3: {
    int3 vec;
    for (size_t i = 0; i < 3; i++)
      vec[i] = int(var.payload.int3Value[i]);
    return vec;
  }
  case SHType::Int4: {
    int4 vec;
    for (size_t i = 0; i < 4; i++)
      vec[i] = int(var.payload.int4Value[i]);
    return vec;
  }
  case SHType::Seq:
    if (isTypedSeq(SHType::Float4)) {
      float4x4 matrix;
      const SHSeq &seq = var.payload.seqValue;
      for (size_t i = 0; i < std::min<size_t>(4, seq.len); i++) {
        float4 row;
        memcpy(&row.x, &seq.elements[i].payload.float4Value, sizeof(float) * 4);
        matrix[i] = row;
      }
      return matrix;
    } else {
      throw formatException("Seq inner value {} can not be converted to NumParameter", var);
    }
  case SHType::Object: {
    auto objType = shards::Type::Object(var.payload.objectVendorId, var.payload.objectTypeId);
    if (objType == ShardsTypes::Buffer) {
      return varAsObjectChecked<SHBuffer>(var, ShardsTypes::Buffer).buffer;
    }
    return TextureParameter(varToTexture(var));
  }
  default:
    throw formatException("Value {} can not be converted to NumParameter", var);
  }
}

inline std::optional<shader::Type> deriveShaderFieldType(const SHTypeInfo &typeInfo) {
  using namespace shader;
  switch (typeInfo.basicType) {
  case SHType::Float:
    return Types::Float;
  case SHType::Float2:
    return Types::Float2;
  case SHType::Float3:
    return Types::Float3;
  case SHType::Float4:
    return Types::Float4;
  case SHType::Int:
    return Types::Int32;
  case SHType::Int2:
    return Types::Int2;
  case SHType::Int3:
    return Types::Int3;
  case SHType::Int4:
    return Types::Int4;
  case SHType::Seq:
    if (typeInfo.seqTypes.len == 1 && typeInfo.seqTypes.elements[0].basicType == SHType::Float4) {
      return Types::Float4x4;
    }
    break;
  case SHType::Object: {
    if (typeInfo == ShardsTypes::Sampler)
      return SamplerType{};
    else if (typeInfo == ShardsTypes::Texture)
      return TextureType(TextureDimension::D2);
    else if (typeInfo == ShardsTypes::TextureCube)
      return TextureType(TextureDimension::Cube);
  }
  default:
    break;
  }
  return std::nullopt;
}

inline std::optional<ShaderParamVariant> tryVarToParam(const SHVar &var) {
  try {
    return varToShaderParameter(var);
  } catch (std::exception &e) {
    SHLOG_ERROR("{}", e.what());
    return std::nullopt;
  }
}

inline void initShaderParams(SHContext *shContext, const SHTable &paramsTable, MaterialParameters &out) {
  shards::ForEach(paramsTable, [&](SHVar &key, SHVar v) {
    if (key.valueType != SHType::String) {
      throw formatException("Invalid shader parameter key type: {}", key.valueType);
    }
    auto kv = SHSTRVIEW(key);

    shards::ParamVar paramVar{v};
    paramVar.warmup(shContext);
    DEFER({ paramVar.cleanup(); });
    SHVar value = paramVar.get();

    auto param = tryVarToParam(value);
    if (param) {
      std::visit([&](auto &&arg) { out.set(kv, arg); }, std::move(param.value()));
    }
  });
}

// Same as initShaderParams but only initializes the parameters that have changed and returns true
inline bool initShaderParamsIfChanged(SHContext *shContext, const SHTable &paramsTable, MaterialParameters &out) {
  bool changed = false;
  shards::ForEach(paramsTable, [&](SHVar &key, SHVar v) {
    if (key.valueType != SHType::String) {
      throw formatException("Invalid shader parameter key type: {}", key.valueType);
    }
    auto kv = SHSTRVIEW(key);

    shards::ParamVar paramVar{v};
    paramVar.warmup(shContext);
    DEFER({ paramVar.cleanup(); });
    SHVar value = paramVar.get();

    auto param = tryVarToParam(value);
    if (param) {
      std::visit(
          [&](auto arg) {
            if (out.setIfChanged(kv, arg)) {
              changed = true;
            }
          },
          std::move(param.value()));
    }
  });
  return changed;
}

} // namespace gfx

#endif /* C7AA640C_1748_42BC_8772_0B8F4E136CEA */

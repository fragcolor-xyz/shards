#ifndef SH_EXTRA_GFX_SHADER_TRANSLATOR_UTILS
#define SH_EXTRA_GFX_SHADER_TRANSLATOR_UTILS

#include "shards/shared.hpp"
#include "translator.hpp"
#include <gfx/shader/wgsl_mapping.hpp>

namespace gfx {
namespace shader {

inline std::unique_ptr<IWGSLGenerated> translateConst(const SHVar &var, TranslationContext &context) {
  std::unique_ptr<IWGSLGenerated> result;

#define OUTPUT_VEC(_type, _dim, _fmt, ...)                                                             \
  {                                                                                                    \
    FieldType fieldType(_type, _dim);                                                                  \
    std::string resultStr = fmt::format("{}(" _fmt ")", getFieldWGSLTypeName(fieldType), __VA_ARGS__); \
    SPDLOG_LOGGER_INFO(context.logger, "gen(const)> {}", resultStr);                                   \
    result = std::make_unique<WGSLSource>(fieldType, std::move(resultStr));                            \
  }

  const SHVarPayload &pl = var.payload;
  SHType valueType = var.valueType;
  switch (valueType) {
  case SHType::Int:
    OUTPUT_VEC(ShaderFieldBaseType::Int32, 1, "{}", pl.intValue);
    break;
  case SHType::Int2:
    OUTPUT_VEC(ShaderFieldBaseType::Int32, 2, "{}, {}", (int32_t)pl.int2Value[0], (int32_t)pl.int2Value[1]);
    break;
  case SHType::Int3:
    OUTPUT_VEC(ShaderFieldBaseType::Int32, 3, "{}, {}, {}", (int32_t)pl.int3Value[0], (int32_t)pl.int3Value[1],
               (int32_t)pl.int3Value[2]);
    break;
  case SHType::Int4:
    OUTPUT_VEC(ShaderFieldBaseType::Int32, 4, "{}, {}, {}, {}", (int32_t)pl.int3Value[0], (int32_t)pl.int3Value[1],
               (int32_t)pl.int3Value[2], (int32_t)pl.int3Value[3]);
    break;
  case SHType::Float:
    OUTPUT_VEC(ShaderFieldBaseType::Float32, 1, "{:f}", (float)pl.floatValue);
    break;
  case SHType::Float2:
    OUTPUT_VEC(ShaderFieldBaseType::Float32, 2, "{:f}, {:f}", (float)pl.float2Value[0], (float)pl.float2Value[1]);
    break;

  case SHType::Float3:
    OUTPUT_VEC(ShaderFieldBaseType::Float32, 3, "{:f}, {:f}, {:f}", (float)pl.float3Value[0], (float)pl.float3Value[1],
               (float)pl.float3Value[2]);
    break;
  case SHType::Float4:
    OUTPUT_VEC(ShaderFieldBaseType::Float32, 4, "{:f}, {:f}, {:f}, {:f}", (float)pl.float4Value[0], (float)pl.float4Value[1],
               (float)pl.float4Value[2], (float)pl.float4Value[3]);
    break;
  default:
    throw ShaderComposeError(fmt::format("Unsupported SHType constant inside shader: {}", magic_enum::enum_name(valueType)));
  }
#undef OUTPUT_VEC
  return result;
};

inline shards::Type fieldTypeToShardsType(const FieldType &type) {
  using shards::CoreInfo;
  if (type.baseType == ShaderFieldBaseType::Float32) {
    switch (type.numComponents) {
    case 1:
      return CoreInfo::FloatType;
    case 2:
      return CoreInfo::Float2Type;
    case 3:
      return CoreInfo::Float3Type;
    case 4:
      return CoreInfo::Float4Type;
    case FieldType::Float4x4Components:
      return CoreInfo::Float4x4Type;
    default:
      throw std::out_of_range(NAMEOF(FieldType::numComponents).str());
    }
  } else {
    switch (type.numComponents) {
    case 1:
      return CoreInfo::IntType;
    case 2:
      return CoreInfo::Int2Type;
    case 3:
      return CoreInfo::Int3Type;
    case 4:
      return CoreInfo::Int4Type;
    default:
      throw std::out_of_range(NAMEOF(FieldType::numComponents).str());
    }
  }
}
} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_TRANSLATOR_UTILS

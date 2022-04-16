#pragma once

// Required before shard headers
#include "shards/shared.hpp"
#include "translator.hpp"
#include <gfx/shader/wgsl_mapping.hpp>

namespace gfx {
namespace shader {
using shards::CoreInfo;

static inline std::string convertVariableName(std::string_view varName) {
  std::string result;
  result.reserve(varName.size());
  for (size_t i = 0; i < varName.size(); i++) {
    char c = varName[i];
    if (c == '-') {
      c = '_';
    }
    result.push_back(c);
  }
  return result;
}

static inline std::unique_ptr<IWGSLGenerated> translateConst(const SHVar &var, TranslationContext &context) {
  std::unique_ptr<IWGSLGenerated> result;

#define OUTPUT_VEC(_type, _dim, _fmt, ...)                                                             \
  {                                                                                                    \
    FieldType fieldType(_type, _dim);                                                                  \
    std::string resultStr = fmt::format("{}(" _fmt ")", getFieldWGSLTypeName(fieldType), __VA_ARGS__); \
    SPDLOG_LOGGER_INFO(&context.logger, "gen(const)> {}", resultStr);                                  \
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

static inline std::unique_ptr<IWGSLGenerated> referenceGlobal(const char *inVarName, TranslationContext &context) {
  auto varName = convertVariableName(inVarName);
  auto it = context.globals.find(varName);
  if (it == context.globals.end()) {
    throw ShaderComposeError(fmt::format("Global value {} not defined", varName));
  }
  return std::make_unique<WGSLBlock>(it->second, blocks::makeBlock<blocks::ReadGlobal>(varName));
}

} // namespace shader
} // namespace gfx

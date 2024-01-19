#ifndef SH_EXTRA_GFX_SHADER_TRANSLATOR_UTILS
#define SH_EXTRA_GFX_SHADER_TRANSLATOR_UTILS

#include <shards/shards.h>
#include <shards/number_types.hpp>
#include <shards/core/shared.hpp>
#include "translator.hpp"
#include "../shards_types.hpp"
#include <gfx/shader/wgsl_mapping.hpp>
#include <stdexcept>

namespace gfx {
namespace shader {

inline std::unique_ptr<IWGSLGenerated> translateConst(const SHVar &var, TranslationContext &context);
inline std::unique_ptr<IWGSLGenerated> translateTable(const SHVar &var, TranslationContext &context) {
  VirtualTable vt;
  shards::ForEach(var.payload.tableValue, [&](const SHVar &key, const SHVar &value) {
    if(key.valueType != SHType::String)
      throw std::runtime_error("Table key must be string");
    std::string keyStr(key.payload.stringValue, key.payload.stringLen);
    std::unique_ptr<IWGSLGenerated> generatedValue;
    if (value.valueType == SHType::ContextVar)
      generatedValue = std::make_unique<WGSLBlock>(context.reference(value.payload.stringValue)); // null term fine cos table values are cloned
    else
      generatedValue = translateConst(value, context);
    vt.elements.emplace(std::make_pair(std::move(keyStr), std::move(generatedValue)));
  });

  return std::make_unique<VirtualTableOnStack>(std::move(vt));
}

inline std::unique_ptr<IWGSLGenerated> translateConst(const SHVar &var, TranslationContext &context) {
  std::unique_ptr<IWGSLGenerated> result{};

#define OUTPUT_VEC(_type, _dim, _fmt, ...)                                                             \
  {                                                                                                    \
    NumType fieldType(_type, _dim);                                                               \
    std::string resultStr = fmt::format("{}(" _fmt ")", getWGSLTypeName(fieldType), __VA_ARGS__); \
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
  case SHType::Table:
    translateTable(var, context);
    break;
  case SHType::None:
    SPDLOG_LOGGER_INFO(context.logger, "gen(const)> nil");
    break;
  default:
    throw ShaderComposeError(fmt::format("Unsupported SHType constant inside shader: {}", magic_enum::enum_name(valueType)));
  }
#undef OUTPUT_VEC

  return result;
};

inline shards::Type fieldTypeToShardsType(const NumType &type) {
  using shards::CoreInfo;
  if (type.baseType == ShaderFieldBaseType::Float32) {
    if (type.matrixDimension > 1) {
      if (type.numComponents == type.matrixDimension && type.matrixDimension == 4) {
        return CoreInfo::Float4x4Type;
      } else if (type.numComponents == type.matrixDimension && type.matrixDimension == 3) {
        return CoreInfo::Float3x3Type;
      } else if (type.numComponents == type.matrixDimension && type.matrixDimension == 2) {
        return CoreInfo::Float2x2Type;
      }
      throw std::out_of_range("Unsupported matrix dimensions");
    } else {
      switch (type.numComponents) {
      case 1:
        return CoreInfo::FloatType;
      case 2:
        return CoreInfo::Float2Type;
      case 3:
        return CoreInfo::Float3Type;
      case 4:
        return CoreInfo::Float4Type;
      default:
        throw std::out_of_range(NAMEOF(NumType::numComponents).str());
      }
    }
  } else {
    if (type.matrixDimension > 1)
      throw std::out_of_range("Integer matrix unsupported");

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
      throw std::out_of_range(NAMEOF(NumType::numComponents).str());
    }
  }
}

inline shards::Type fieldTypeToShardsType(const TextureType &type) {
  switch (type.dimension) {
  case TextureDimension::D2:
    return ShardsTypes::Texture;
  case TextureDimension::Cube:
    return ShardsTypes::TextureCube;
  default:
    throw std::logic_error("Texture dimension unsupported");
  }
}

inline shards::Type fieldTypeToShardsType(const SamplerType &type) { return ShardsTypes::Sampler; }

inline shards::Type fieldTypeToShardsType(const Type &type) {
  return std::visit([&](auto &arg) -> shards::Type { return fieldTypeToShardsType(arg); }, type);
}

static constexpr const char componentNames[] = {'x', 'y', 'z', 'w'};

inline constexpr ShaderFieldBaseType getShaderBaseType(shards::NumberType numberType) {
  using shards::NumberType;
  switch (numberType) {
  case NumberType::Float32:
  case NumberType::Float64:
    return ShaderFieldBaseType::Float32;
  case NumberType::Int64:
  case NumberType::Int32:
  case NumberType::Int16:
  case NumberType::Int8:
  case NumberType::UInt8:
    return ShaderFieldBaseType::Int32;
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(shards::NumberType)));
  }
}

inline Type shardsTypeToFieldType(const SHTypeInfo &typeInfo) {
  SHType basicType = typeInfo.basicType;
  if (basicType == SHType::Seq) {
    if (typeInfo.seqTypes.len != 1)
      throw std::runtime_error(fmt::format("Type {} is not a valid shader type (only square matrices supported)", typeInfo));

    const shards::VectorTypeTraits *type = shards::VectorTypeLookup::getInstance().get(typeInfo.basicType);
    if (!type)
      throw std::runtime_error(fmt::format("Type {} is not a valid matrix row type", typeInfo));

    NumType fieldType = getShaderBaseType(type->numberType);
    fieldType.numComponents = type->dimension;
    fieldType.matrixDimension = typeInfo.fixedSize;
    return fieldType;
  } else {
    const shards::VectorTypeTraits *type = shards::VectorTypeLookup::getInstance().get(typeInfo.basicType);
    if (!type)
      throw std::runtime_error(fmt::format("Type {} is not supported in this shader context", typeInfo));

    NumType fieldType = getShaderBaseType(type->numberType);
    fieldType.numComponents = type->dimension;
    return fieldType;
  }
}

inline std::unique_ptr<IWGSLGenerated> translateParamVar(const shards::ParamVar &var, TranslationContext &context) {
  if (var.isVariable()) {
    return std::make_unique<WGSLBlock>(context.reference(var.variableName()));
  } else {
    return translateConst(var, context);
  }
}

inline void processShardsVar(shards::ShardsVar &shards, TranslationContext &context) {
  auto &shardsSeq = shards.shards();
  for (size_t i = 0; i < shardsSeq.len; i++) {
    ShardPtr shard = shardsSeq.elements[i];
    context.processShard(shard);
  }
}

inline std::unique_ptr<IWGSLGenerated> generateFunctionCall(const TranslatedFunction &function,
                                                            const std::unique_ptr<IWGSLGenerated> &input,
                                                            TranslationContext &context) {
  BlockPtr inputCallArg;

  if (function.inputType) {
    assert(input);
    inputCallArg = input->toBlock();
  }

  // Build function call argument list
  auto callBlock = blocks::makeCompoundBlock();
  callBlock->append(fmt::format("{}(", function.functionName));

  bool addSeparator{};

  // Pass input as first argument
  if (inputCallArg) {
    callBlock->append(std::move(inputCallArg));
    addSeparator = true;
  }

  // Pass captured variables
  if (!function.arguments.empty()) {
    for (auto &arg : function.arguments) {
      if (addSeparator)
        callBlock->append(", ");
      callBlock->append(context.reference(arg.shardsName).toBlock());
      addSeparator = true;
    }
  }

  callBlock->append(")");

  Type outputType = function.outputType ? function.outputType.value() : Types::Float;
  return std::make_unique<WGSLBlock>(outputType, std::move(callBlock));
}

} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_TRANSLATOR_UTILS

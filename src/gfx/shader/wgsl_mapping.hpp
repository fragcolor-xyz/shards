#pragma once
#include "types.hpp"
#include <cassert>
#include <gfx/params.hpp>
#include <spdlog/fmt/fmt.h>

namespace gfx {
namespace shader {

inline ShaderFieldBaseType getCompatibleShaderFieldBaseType(VertexAttributeType type) {
  switch (type) {
  case VertexAttributeType::UInt8:
    return ShaderFieldBaseType::UInt8;
  case VertexAttributeType::Int8:
    return ShaderFieldBaseType::Int8;
  case VertexAttributeType::UInt16:
    return ShaderFieldBaseType::UInt16;
  case VertexAttributeType::Int16:
    return ShaderFieldBaseType::Int16;
  case VertexAttributeType::UNorm8:
  case VertexAttributeType::SNorm8:
  case VertexAttributeType::UNorm16:
  case VertexAttributeType::SNorm16:
    return ShaderFieldBaseType::Float32;
  case VertexAttributeType::UInt32:
    return ShaderFieldBaseType::UInt32;
  case VertexAttributeType::Int32:
    return ShaderFieldBaseType::Int32;
  case VertexAttributeType::Float16:
    return ShaderFieldBaseType::Float16;
  case VertexAttributeType::Float32:
    return ShaderFieldBaseType::Float32;
  default:
    assert(false);
    return ShaderFieldBaseType(~0);
  }
}

inline const char *getParamWGSLTypeName(ShaderParamType type) {
  switch (type) {
  case ShaderParamType::Float:
    return "f32";
  case ShaderParamType::Float2:
    return "vec2<f32>";
  case ShaderParamType::Float3:
    return "vec3<f32>";
  case ShaderParamType::Float4:
    return "vec4<f32>";
  case ShaderParamType::Float4x4:
    return "mat4x4<f32>";
    break;
  default:
    assert(false);
    break;
  }
}
static String getFieldWGSLTypeName(const FieldType &type) {
  const char *baseType = nullptr;
  switch (type.baseType) {
  case ShaderFieldBaseType::UInt8:
    baseType = "u8";
    break;
  case ShaderFieldBaseType::Int8:
    baseType = "i8";
    break;
  case ShaderFieldBaseType::UInt16:
    baseType = "f32";
    break;
  case ShaderFieldBaseType::Int16:
    baseType = "i16";
    break;
  case ShaderFieldBaseType::UInt32:
    baseType = "u32";
    break;
  case ShaderFieldBaseType::Int32:
    baseType = "i32";
    break;
  case ShaderFieldBaseType::Float16:
    baseType = "f32";
    break;
  case ShaderFieldBaseType::Float32:
    baseType = "f32";
    break;
  default:
    assert(false);
    break;
  }
  const char *vecType = nullptr;
  switch (type.numComponents) {
  default:
  case 1:
    break;
  case 2:
    vecType = "vec2";
    break;
  case 3:
    vecType = "vec3";
    break;
  case 4:
    vecType = "vec4";
    break;
  }
  return vecType ? fmt::format("{}<{}>", vecType, baseType) : baseType;
}
} // namespace shader
} // namespace gfx

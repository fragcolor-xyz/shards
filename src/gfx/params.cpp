#include "params.hpp"
#include "linalg/linalg.h"
#include "spdlog/fmt/bundled/core.h"
#include <cassert>
#include <nameof.hpp>
#include <spdlog/fmt/fmt.h>
#include <type_traits>

namespace gfx {

struct PackParamVisitor {
  uint8_t *outData{};
  size_t outLength{};

  size_t operator()(const uint32_t &arg) { return packPlain(arg); }
  size_t operator()(const float &arg) { return packPlain(arg); }
  size_t operator()(const float2 &arg) {
    size_t len = sizeof(float) * 2;
    assert(len <= outLength);
    memcpy(outData, &arg.x, len);
    return len;
  }
  size_t operator()(const float3 &arg) {
    size_t len = sizeof(float) * 3;
    assert(len <= outLength);
    memcpy(outData, &arg.x, len);
    return len;
  }
  size_t operator()(const float4 &arg) {
    size_t len = sizeof(float) * 4;
    assert(len <= outLength);
    memcpy(outData, &arg.x, len);
    return len;
  }
  size_t operator()(const float4x4 &arg) {
    size_t len = sizeof(float) * 16;
    assert(len <= outLength);
    packFloat4x4(arg, (float *)outData);
    return len;
  }

  template <typename T> size_t packPlain(const T &val) {
    size_t len = sizeof(T);
    assert(len <= outLength);
    memcpy(outData, &val, len);
    return len;
  }

  size_t operator()(std::monostate) { return 0; }
};

size_t packParamVariant(uint8_t *outData, size_t outLength, const ParamVariant &variant) {
  PackParamVisitor visitor{outData, outLength};
  return std::visit(visitor, variant);
}

using shader::FieldType;
FieldType getParamVariantType(const ParamVariant &variant) {
  FieldType result = {};
  std::visit(
      [&](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, float>) {
          result = FieldType(ShaderFieldBaseType::Float32);
        } else if constexpr (std::is_same_v<T, float2>) {
          result = FieldType(ShaderFieldBaseType::Float32, 2);
        } else if constexpr (std::is_same_v<T, float3>) {
          result = FieldType(ShaderFieldBaseType::Float32, 3);
        } else if constexpr (std::is_same_v<T, float4>) {
          result = FieldType(ShaderFieldBaseType::Float32, 4);
        } else if constexpr (std::is_same_v<T, float4x4>) {
          result = FieldType(ShaderFieldBaseType::Float32, FieldType::Float4x4Components);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
          result = FieldType(ShaderFieldBaseType::UInt32, 1);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
          result = FieldType(ShaderFieldBaseType::UInt16, 1);
        } else if constexpr (std::is_same_v<T, uint8_t>) {
          result = FieldType(ShaderFieldBaseType::UInt8, 1);
        } else {
          throw std::logic_error(fmt::format("Type {} not suported by ParamVariant", NAMEOF_TYPE(T)));
        }
      },
      variant);
  return result;
}

} // namespace gfx

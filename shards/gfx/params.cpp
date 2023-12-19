#include "params.hpp"
#include "linalg/linalg.h"
#include "spdlog/fmt/bundled/core.h"
#include "../core/assert.hpp"
#include <nameof.hpp>
#include <spdlog/fmt/fmt.h>
#include <type_traits>

namespace gfx {

struct PackParamVisitor {
  uint8_t *outData{};
  size_t outLength{};

  size_t operator()(const float &arg) { return packPlain(arg); }
  size_t operator()(const float2 &arg) {
    size_t len = sizeof(float) * 2;
    shassert(len <= outLength);
    memcpy(outData, &arg.x, len);
    return len;
  }
  size_t operator()(const float3 &arg) {
    size_t len = sizeof(float) * 3;
    shassert(len <= outLength);
    memcpy(outData, &arg.x, len);
    return len;
  }
  size_t operator()(const float4 &arg) {
    size_t len = sizeof(float) * 4;
    shassert(len <= outLength);
    memcpy(outData, &arg.x, len);
    return len;
  }
  size_t operator()(const float2x2 &arg) {
    size_t len = getPackedMatrixSize(arg);
    shassert(len <= outLength);
    packMatrix(arg, (float *)outData);
    return len;
  }
  size_t operator()(const float3x3 &arg) {
    size_t len = getPackedMatrixSize(arg);
    shassert(len <= outLength);
    packMatrix(arg, (float *)outData);
    return len;
  }
  size_t operator()(const float4x4 &arg) {
    size_t len = getPackedMatrixSize(arg);
    shassert(len <= outLength);
    packMatrix(arg, (float *)outData);
    return len;
  }
  size_t operator()(const uint32_t &arg) { return packPlain(arg); }
  size_t operator()(const int32_t &arg) {
    size_t len = sizeof(int32_t);
    shassert(len <= outLength);
    memcpy(outData, &arg, len);
    return len;
  }
  size_t operator()(const int2 &arg) {
    size_t len = sizeof(int32_t) * 2;
    shassert(len <= outLength);
    memcpy(outData, &arg.x, len);
    return len;
  }
  size_t operator()(const int3 &arg) {
    size_t len = sizeof(int32_t) * 3;
    shassert(len <= outLength);
    memcpy(outData, &arg.x, len);
    return len;
  }
  size_t operator()(const int4 &arg) {
    size_t len = sizeof(int32_t) * 4;
    shassert(len <= outLength);
    memcpy(outData, &arg.x, len);
    return len;
  }
  template <typename T> size_t packPlain(const T &val) {
    size_t len = sizeof(T);
    shassert(len <= outLength);
    memcpy(outData, &val, len);
    return len;
  }

  size_t operator()(std::monostate) { return 0; }
};

size_t packNumParameter(uint8_t *outData, size_t outLength, const NumParameter &variant) {
  PackParamVisitor visitor{outData, outLength};
  return std::visit(visitor, variant);
}

using shader::FieldTypes;
using shader::NumFieldType;
NumFieldType getNumParameterType(const NumParameter &variant) {
  NumFieldType result = {};
  std::visit(
      [&](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, float>) {
          result = FieldTypes::Float;
        } else if constexpr (std::is_same_v<T, float2>) {
          result = FieldTypes::Float2;
        } else if constexpr (std::is_same_v<T, float3>) {
          result = FieldTypes::Float3;
        } else if constexpr (std::is_same_v<T, float4>) {
          result = FieldTypes::Float4;
        } else if constexpr (std::is_same_v<T, float4x4>) {
          result = FieldTypes::Float4x4;
        } else if constexpr (std::is_same_v<T, int>) {
          result = FieldTypes::Int32;
        } else if constexpr (std::is_same_v<T, int2>) {
          result = FieldTypes::Int2;
        } else if constexpr (std::is_same_v<T, int3>) {
          result = FieldTypes::Int3;
        } else if constexpr (std::is_same_v<T, int4>) {
          result = FieldTypes::Int4;
        } else if constexpr (std::is_same_v<T, uint32_t>) {
          result = FieldTypes::UInt32;
        } else {
          throw std::logic_error(fmt::format("Type {} not suported by NumParameter", NAMEOF_TYPE(T)));
        }
      },
      variant);
  return result;
}

} // namespace gfx

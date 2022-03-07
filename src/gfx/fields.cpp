#include "fields.hpp"
#include "linalg/linalg.h"
#include "spdlog/fmt/bundled/core.h"
#include <cassert>
#include <spdlog/fmt/fmt.h>
#include <type_traits>

namespace gfx {

size_t packFieldVariant(uint8_t *outData, size_t outLength, const FieldVariant &variant) {
  return std::visit(
      [&](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, float>) {
          size_t len = sizeof(float) * 1;
          assert(len <= outLength);
          memcpy(outData, &arg, len);
          return len;
        } else if constexpr (std::is_same_v<T, float2>) {
          size_t len = sizeof(float) * 2;
          assert(len <= outLength);
          memcpy(outData, &arg.x, len);
          return len;
        } else if constexpr (std::is_same_v<T, float3>) {
          size_t len = sizeof(float) * 3;
          assert(len <= outLength);
          memcpy(outData, &arg.x, len);
          return len;
        } else if constexpr (std::is_same_v<T, float4>) {
          size_t len = sizeof(float) * 4;
          assert(len <= outLength);
          memcpy(outData, &arg.x, len);
          return len;
        } else if constexpr (std::is_same_v<T, float4x4>) {
          size_t len = sizeof(float) * 16;
          assert(len <= outLength);
          packFloat4x4(arg, (float *)outData);
          return len;
        }
      },
      variant);
}

FieldType getFieldVariantType(const FieldVariant &variant) {
  FieldType result = {};
  std::visit(
      [&](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;

        if (false) {
          // ignored
        }
#define FIELD_TYPE(_cppType, _displayName, ...)     \
  else if constexpr (std::is_same_v<T, _cppType>) { \
    result = FieldType::_displayName;               \
  }
#include "field_types.def"
#undef FIELD_TYPE
        else {
          assert(false);
        }
      },
      variant);
  return result;
}

size_t getFieldTypeSize(FieldType type) {
  switch (type) {
#define FIELD_TYPE(_cppType, _displayName, _size, ...) \
  case FieldType::_displayName: {                      \
    return _size;                                      \
  } break;
#include "field_types.def"
#undef FIELD_TYPE
  default:
    assert(false);
    return ~0;
  }
}

size_t getFieldTypeWGSLAlignment(FieldType type) {
  switch (type) {
#define FIELD_TYPE(_0, _displayName, _1, _alignment, ...) \
  case FieldType::_displayName: {                         \
    return _alignment;                                    \
  } break;
#include "field_types.def"
#undef FIELD_TYPE
  default:
    assert(false);
    return ~0;
  }
}

} // namespace gfx

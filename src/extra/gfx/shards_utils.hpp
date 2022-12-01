#ifndef EXTRA_GFX_SHARDS_UTILS
#define EXTRA_GFX_SHARDS_UTILS

#include <foundation.hpp>
#include <gfx/error_utils.hpp>
#include <gfx/linalg.hpp>
#include <magic_enum.hpp>
#include <shards.hpp>

namespace gfx {
// Retrieves a value directly or from a context variable from a table by name
// returns false if the table does not contain an entry for that key
inline bool getFromTable(SHContext *shContext, const SHTable &table, const char *key, SHVar &outVar) {
  if (table.api->tableContains(table, key)) {
    const SHVar *var = table.api->tableAt(table, key);
    if (var->valueType == SHType::ContextVar) {
      SHVar *refencedVariable = shards::referenceVariable(shContext, var->payload.stringValue);
      outVar = *var;
      shards::releaseVariable(refencedVariable);
    }
    outVar = *var;
    return true;
  }
  return false;
}

inline void checkType(const SHType &type, SHType expectedType, const char *name) {
  if (type != expectedType)
    throw formatException("{} type should be {}, was {}", name, magic_enum::enum_name(expectedType), magic_enum::enum_name(type));
}

inline void checkEnumType(const SHVar &var, const shards::Type &expectedType, const char *name) {
  checkType(var.valueType, SHType::Enum, name);
  shards::Type actualType = shards::Type::Enum(var.payload.enumVendorId, var.payload.enumTypeId);
  if (expectedType != actualType) {
    SHTypeInfo typeInfoA = expectedType;
    SHTypeInfo typeInfoB = actualType;
    throw formatException("{} enum type should be {}/{}, was {}/{}", name, typeInfoA.enumeration.vendorId,
                          typeInfoA.enumeration.typeId, typeInfoB.enumeration.vendorId, typeInfoB.enumeration.typeId);
  }
}

template <typename T> struct VectorConversion {};
template <> struct VectorConversion<float> {
  static auto convert(const SHVar &value) {
    if (value.valueType != SHType::Float)
      throw std::runtime_error("Invalid vector type");
    return value.payload.floatValue;
  }
};
template <> struct VectorConversion<float2> {
  static auto convert(const SHVar &value) {
    if (value.valueType != SHType::Float2)
      throw std::runtime_error("Invalid vector type");
    return float2(value.payload.float2Value[0], value.payload.float2Value[1]);
  }
};
template <> struct VectorConversion<float3> {
  static auto convert(const SHVar &value) {
    if (value.valueType != SHType::Float3)
      throw std::runtime_error("Invalid vector type");
    return float3(value.payload.float3Value[0], value.payload.float3Value[1], value.payload.float3Value[2]);
  }
};
template <> struct VectorConversion<float4> {
  static auto convert(const SHVar &value) {
    if (value.valueType != SHType::Float4)
      throw std::runtime_error("Invalid vector type");
    return float4(value.payload.float4Value[0], value.payload.float4Value[1], value.payload.float4Value[2], value.payload.float4Value[3]);
  }
};

template <typename TVec> inline auto toVec(const SHVar &value) { return VectorConversion<TVec>::convert(value); }

} // namespace gfx

#endif // EXTRA_GFX_SHARDS_UTILS

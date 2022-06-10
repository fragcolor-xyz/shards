#ifndef EXTRA_GFX_SHARDS_UTILS
#define EXTRA_GFX_SHARDS_UTILS

#include <foundation.hpp>
#include <gfx/error_utils.hpp>
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
} // namespace gfx

#endif // EXTRA_GFX_SHARDS_UTILS

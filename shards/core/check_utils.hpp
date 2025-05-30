#ifndef C25C0EEE_CC07_432E_9764_0EC84FA97064
#define C25C0EEE_CC07_432E_9764_0EC84FA97064

#include <shards/shards.hpp>
#include "exception.hpp"

namespace shards {
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
} // namespace shards

#endif /* C25C0EEE_CC07_432E_9764_0EC84FA97064 */

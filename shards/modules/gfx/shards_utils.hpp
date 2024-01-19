#ifndef AD2CA4AE_4D00_49A0_8DD6_323B82813690
#define AD2CA4AE_4D00_49A0_8DD6_323B82813690

#include <shards/core/foundation.hpp>
#include <shards/utility.hpp>
#include <gfx/error_utils.hpp>
#include <gfx/linalg.hpp>
#include <gfx/fwd.hpp>
#include <magic_enum.hpp>
#include <shards/shards.hpp>
#include <string_view>
#include <shards/shards.h>
#include "shards_types.hpp"

namespace gfx {
// Retrieves a value directly or from a context variable from a table by name
// returns false if the table does not contain an entry for that key
inline bool getFromTable(SHContext *shContext, const SHTable &table, const SHVar &key, SHVar &outVar) {
  if (table.api->tableContains(table, key)) {
    const SHVar *var = table.api->tableAt(table, key);
    if (var->valueType == SHType::ContextVar) {
      SHVar *refencedVariable = shards::referenceVariable(shContext, SHSTRVIEW((*var)));
      outVar = *refencedVariable;
      shards::releaseVariable(refencedVariable);
    } else {
      outVar = *var;
    }
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

inline void applyFeatures(SHContext *context, std::vector<FeaturePtr> &outFeatures, const SHVar &input) {
  checkType(input.valueType, SHType::Seq, ":Features");
  for (size_t i = 0; i < input.payload.seqValue.len; i++) {
    auto &elem = input.payload.seqValue.elements[i];
    outFeatures.push_back(varAsObjectChecked<FeaturePtr>(elem, ShardsTypes::Feature));
  }
}

inline bool applyFeaturesIfChanged(SHContext *context, std::vector<FeaturePtr> &outFeatures, const SHVar &input) {
  checkType(input.valueType, SHType::Seq, ":Features");
  bool changed = false;
  if (input.payload.seqValue.len != outFeatures.size()) {
    changed = true;
  }

  outFeatures.resize(input.payload.seqValue.len);
  for (size_t i = 0; i < input.payload.seqValue.len; i++) {
    auto &elem = input.payload.seqValue.elements[i];
    auto &newFeature = varAsObjectChecked<FeaturePtr>(elem, ShardsTypes::Feature);
    auto &outFeature = outFeatures[i];
    if (outFeature != newFeature) {
      outFeature = newFeature;
      changed = true;
      break;
    }
  }
  return changed;
}

} // namespace gfx

#endif /* AD2CA4AE_4D00_49A0_8DD6_323B82813690 */

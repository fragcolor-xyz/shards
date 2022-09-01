#ifndef A17B36D4_990D_426A_8EFF_DA2CBDF19CB2
#define A17B36D4_990D_426A_8EFF_DA2CBDF19CB2

#include <shards.hpp>
#include <cassert>

namespace shards {

// Evaluates inner with a variable set to the given value & type
//  and restores it before returning
template <typename TInner>
inline void withObjectVariable(SHVar &var, void *objectValue, const SHTypeInfo &typeInfo, TInner &&inner) {
  void *previousObjectValue = var.payload.objectValue;

  // Set object
  assert(typeInfo.basicType == SHType::Object);
  var.payload.objectValue = objectValue;
  var.payload.objectTypeId = typeInfo.object.typeId;
  var.payload.objectVendorId = typeInfo.object.vendorId;
  var.valueType = SHType::Object;

  inner();

  // Restore
  var.payload.objectValue = previousObjectValue;
  if (!var.payload.objectValue)
    var.valueType = SHType::None;
}
} // namespace shards

#endif /* A17B36D4_990D_426A_8EFF_DA2CBDF19CB2 */

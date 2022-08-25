#ifndef A17B36D4_990D_426A_8EFF_DA2CBDF19CB2
#define A17B36D4_990D_426A_8EFF_DA2CBDF19CB2

#include <shards.hpp>
#include <cassert>

namespace shards {

// Sets the value of an object variable and clears when this object goes out of scope
struct ObjectVarScope {
  SHVar &var;

  ObjectVarScope(SHVar &var, void *objectValue, const SHTypeInfo &typeInfo) : var(var) {
    assert(typeInfo.basicType == SHType::Object);
    var.payload.objectValue = objectValue;
    var.payload.objectTypeId = typeInfo.object.typeId;
    var.payload.objectVendorId = typeInfo.object.vendorId;
    var.valueType = SHType::Object;
  }

  ~ObjectVarScope() {
    var.payload.objectValue = nullptr;
    var.valueType = SHType::None;
  }
};
} // namespace shards

#endif /* A17B36D4_990D_426A_8EFF_DA2CBDF19CB2 */

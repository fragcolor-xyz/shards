#include "type_info.hpp"
#include "foundation.hpp"
#include "trait.hpp"
#include <shards/iterator.hpp>

namespace shards {
void freeTypeInfo(SHTypeInfo info) {
  switch (info.basicType) {
  case SHType::Object:
    // for (auto &t : info.object.traits) {
    //   freeTrait(t);
    // }
    // shards::arrayFree(info.object.traits);
    if (info.object.extended) {
      info.object.extended->release(info.object.extended);
    }
    break;
  case SHType::Seq: {
    for (uint32_t i = 0; info.seqTypes.len > i; i++) {
      freeDerivedInfo(info.seqTypes.elements[i]);
    }
    shards::arrayFree(info.seqTypes);
  } break;
  case SHType::Set: {
    for (uint32_t i = 0; info.setTypes.len > i; i++) {
      freeDerivedInfo(info.setTypes.elements[i]);
    }
    shards::arrayFree(info.setTypes);
  } break;
  case SHType::Table: {
    for (uint32_t i = 0; info.table.types.len > i; i++) {
      freeDerivedInfo(info.table.types.elements[i]);
    }
    for (uint32_t i = 0; info.table.keys.len > i; i++) {
      destroyVar(info.table.keys.elements[i]);
    }
    shards::arrayFree(info.table.types);
    shards::arrayFree(info.table.keys);
  } break;
  default:
    break;
  };
}

SHTypeInfo cloneTypeInfo(const SHTypeInfo &other) {
  // We need to guess a valid SHTypeInfo for this var in order to validate
  // Build a SHTypeInfo for the var
  // this is not complete at all, missing Array and SHType::ContextVar for example
  SHTypeInfo varType;
  memcpy(&varType, &other, sizeof(SHTypeInfo));
  switch (varType.basicType) {
  case SHType::Object:
    if (other.object.extended) {
      other.object.extended->reference(other.object.extended);
    }
    break;
  case SHType::ContextVar:
  case SHType::Set:
  case SHType::Seq: {
    varType.seqTypes = {};
    for (uint32_t i = 0; i < other.seqTypes.len; i++) {
      auto cloned = cloneTypeInfo(other.seqTypes.elements[i]);
      shards::arrayPush(varType.seqTypes, cloned);
    }
    break;
  }
  case SHType::Table: {
    varType.table = {};
    for (uint32_t i = 0; i < other.table.types.len; i++) {
      auto cloned = cloneTypeInfo(other.table.types.elements[i]);
      shards::arrayPush(varType.table.types, cloned);
    }
    for (uint32_t i = 0; i < other.table.keys.len; i++) {
      auto idx = varType.table.keys.len;
      shards::arrayResize(varType.table.keys, idx + 1);
      cloneVar(varType.table.keys.elements[idx], other.table.keys.elements[i]);
    }
    break;
  }
  default:
    break;
  };
  return varType;
}

SHTypeInfo deriveTypeInfo(const SHVar &value, const SHInstanceData &data, std::vector<SHExposedTypeInfo> *expInfo,
                          bool resolveContextVariables) {
  // We need to guess a valid SHTypeInfo for this var in order to validate
  // Build a SHTypeInfo for the var
  // this is not complete at all, missing Array and SHType::ContextVar for example
  SHTypeInfo varType{};
  varType.basicType = value.valueType;
  varType.innerType = value.innerType;
  switch (value.valueType) {
  case SHType::Object: {
    varType.object.vendorId = value.payload.objectVendorId;
    varType.object.typeId = value.payload.objectTypeId;
    break;
  }
  case SHType::Enum: {
    varType.enumeration.vendorId = value.payload.enumVendorId;
    varType.enumeration.typeId = value.payload.enumTypeId;
    break;
  }
  case SHType::Seq: {
    std::unordered_set<SHTypeInfo> types;
    for (uint32_t i = 0; i < value.payload.seqValue.len; i++) {
      auto derived = deriveTypeInfo(value.payload.seqValue.elements[i], data, expInfo, resolveContextVariables);
      if (!types.count(derived)) {
        shards::arrayPush(varType.seqTypes, derived);
        types.insert(derived);
      }
    }
    varType.fixedSize = value.payload.seqValue.len;
    break;
  }
  case SHType::Table: {
    auto &t = value.payload.tableValue;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHVar k;
    SHVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      auto derived = deriveTypeInfo(v, data, expInfo, resolveContextVariables);
      shards::arrayPush(varType.table.types, derived);
      auto idx = varType.table.keys.len;
      shards::arrayResize(varType.table.keys, idx + 1);
      cloneVar(varType.table.keys.elements[idx], k);
    }
  } break;
  case SHType::Set: {
    auto &s = value.payload.setValue;
    SHSetIterator sit;
    s.api->setGetIterator(s, &sit);
    SHVar v;
    while (s.api->setNext(s, &sit, &v)) {
      auto derived = deriveTypeInfo(v, data, expInfo, resolveContextVariables);
      shards::arrayPush(varType.setTypes, derived);
    }
  } break;
  case SHType::ContextVar: {
    if (expInfo) {
      auto sv = SHSTRVIEW(value);
      const auto varName = sv;
      for (auto info : data.shared) {
        std::string_view name(info.name);
        if (name == sv) {
          expInfo->push_back(SHExposedTypeInfo{.name = info.name, .exposedType = info.exposedType});
          if (resolveContextVariables) {
            return cloneTypeInfo(info.exposedType);
          } else {
            shards::arrayPush(varType.contextVarTypes, cloneTypeInfo(info.exposedType));
            return varType;
          }
        }
      }
      SHLOG_ERROR("Could not find variable {} when deriving type info", varName);
      throw std::runtime_error("Could not find variable when deriving type info");
    }
    // if we reach this point, no variable was found...
    // not our job to trigger errors, just continue
    // specifically we are used to verify parameters as well
    // and in that case data is empty
  } break;
  default:
    break;
  };
  return varType;
}
} // namespace shards

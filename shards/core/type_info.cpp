#include "type_info.hpp"
#include "foundation.hpp"
#include "trait.hpp"
#include <shards/iterator.hpp>

namespace shards {
void freeTypeInfo(SHTypeInfo info) {
  switch (info.basicType) {
  case SHType::Object:
    if (info.object.extInfo) {
      info.object.extInfo->release(info.object.extInfoData);
    }
    break;
  case SHType::ContextVar: {
    for (uint32_t i = 0; info.contextVarTypes.len > i; i++) {
      freeDerivedInfo(info.contextVarTypes.elements[i]);
    }
    shards::arrayFree(info.contextVarTypes);
  } break;
  case SHType::Set: {
    for (uint32_t i = 0; info.setTypes.len > i; i++) {
      freeDerivedInfo(info.setTypes.elements[i]);
    }
    shards::arrayFree(info.setTypes);
  } break;
  case SHType::Seq: {
    for (uint32_t i = 0; info.seqTypes.len > i; i++) {
      freeDerivedInfo(info.seqTypes.elements[i]);
    }
    shards::arrayFree(info.seqTypes);
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
  }
}

SHTypeInfo cloneTypeInfo(const SHTypeInfo &other) {
  SHTypeInfo varType;
  memcpy(&varType, &other, sizeof(SHTypeInfo));
  switch (varType.basicType) {
  case SHType::Object:
    if (other.object.extInfo) {
      other.object.extInfo->reference(other.object.extInfoData);
    }
    break;
  case SHType::ContextVar: {
    varType.contextVarTypes = {};
    for (uint32_t i = 0; i < other.contextVarTypes.len; i++) {
      auto cloned = cloneTypeInfo(other.contextVarTypes.elements[i]);
      shards::arrayPush(varType.contextVarTypes, cloned);
    }
    break;
  }
  case SHType::Set: {
    varType.setTypes = {};
    for (uint32_t i = 0; i < other.setTypes.len; i++) {
      auto cloned = cloneTypeInfo(other.setTypes.elements[i]);
      shards::arrayPush(varType.setTypes, cloned);
    }
    break;
  }
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
  }
  return varType;
}

SHTypeInfo deriveTypeInfo(const SHVar &value, const SHInstanceData &data, std::vector<SHExposedTypeInfo> *expInfo,
                          bool resolveContextVariables) {
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
    break;
  }
  case SHType::Set: {
    auto &s = value.payload.setValue;
    SHSetIterator sit;
    s.api->setGetIterator(s, &sit);
    SHVar v;
    while (s.api->setNext(s, &sit, &v)) {
      auto derived = deriveTypeInfo(v, data, expInfo, resolveContextVariables);
      shards::arrayPush(varType.setTypes, derived);
    }
    break;
  }
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
  } break;
  default:
    break;
  };
  return varType;
}
} // namespace shards

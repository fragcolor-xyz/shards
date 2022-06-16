/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "ops_internal.hpp"
#include "ops.hpp"
#include "runtime.hpp"
#include <unordered_set>

std::ostream &operator<<(std::ostream &os, const SHVar &var) {
  switch (var.valueType) {
  case SHType::EndOfBlittableTypes:
    break;
  case SHType::Error:
    os << "Error, Code: " << var.payload.errorValue.code << " Message: " << var.payload.errorValue.message;
    break;
  case SHType::None:
    os << "None";
    break;
  case SHType::Any:
    os << "Any";
    break;
  case SHType::Object:
    os << "Object: 0x" << std::hex << reinterpret_cast<uintptr_t>(var.payload.objectValue) << " vendor: 0x"
       << var.payload.objectVendorId << " type: 0x" << var.payload.objectTypeId << std::dec;
    break;
  case SHType::Wire: {
    if (var.payload.wireValue) {
      auto wire = SHWire::sharedFromRef(var.payload.wireValue);
      os << "Wire: 0x" << std::hex << reinterpret_cast<uintptr_t>(var.payload.wireValue) << std::dec;
      os << " name: " << wire->name;
    } else {
      os << "Wire: 0x0";
    }
  } break;
  case SHType::Bytes:
    os << "Bytes: 0x" << std::hex << reinterpret_cast<uintptr_t>(var.payload.bytesValue) << " size: " << std::dec
       << var.payload.bytesSize;
    break;
  case SHType::Array:
    os << "Array: 0x" << std::hex << reinterpret_cast<uintptr_t>(var.payload.arrayValue.elements) << " size: " << std::dec
       << var.payload.arrayValue.len << " of: " << type2Name(var.innerType);
    break;
  case SHType::Enum:
    os << "Enum: " << var.payload.enumValue << std::hex << " vendor: 0x" << var.payload.enumVendorId << " type: 0x"
       << var.payload.enumTypeId << std::dec;
    break;
  case SHType::Bool:
    os << (var.payload.boolValue ? "true" : "false");
    break;
  case SHType::Int:
    os << var.payload.intValue;
    break;
  case SHType::Int2:
    os << "(";
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.int2Value[i];
      else
        os << ", " << var.payload.int2Value[i];
    }
    os << ")";
    break;
  case SHType::Int3:
    os << "(";
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.int3Value[i];
      else
        os << ", " << var.payload.int3Value[i];
    }
    os << ")";
    break;
  case SHType::Int4:
    os << "(";
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.int4Value[i];
      else
        os << ", " << var.payload.int4Value[i];
    }
    os << ")";
    break;
  case SHType::Int8:
    os << "(";
    for (auto i = 0; i < 8; i++) {
      if (i == 0)
        os << var.payload.int8Value[i];
      else
        os << ", " << var.payload.int8Value[i];
    }
    os << ")";
    break;
  case SHType::Int16:
    os << "(";
    for (auto i = 0; i < 16; i++) {
      if (i == 0)
        os << int(var.payload.int16Value[i]);
      else
        os << ", " << int(var.payload.int16Value[i]);
    }
    os << ")";
    break;
  case SHType::Float:
    os << var.payload.floatValue;
    break;
  case SHType::Float2:
    os << "(";
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.float2Value[i];
      else
        os << ", " << var.payload.float2Value[i];
    }
    os << ")";
    break;
  case SHType::Float3:
    os << "(";
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.float3Value[i];
      else
        os << ", " << var.payload.float3Value[i];
    }
    os << ")";
    break;
  case SHType::Float4:
    os << "(";
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.float4Value[i];
      else
        os << ", " << var.payload.float4Value[i];
    }
    os << ")";
    break;
  case SHType::Color:
    os << int(var.payload.colorValue.r) << ", " << int(var.payload.colorValue.g) << ", " << int(var.payload.colorValue.b) << ", "
       << int(var.payload.colorValue.a);
    break;
  case SHType::ShardRef:
    os << "Shard: " << var.payload.shardValue->name(var.payload.shardValue);
    break;
  case SHType::String:
    if (var.payload.stringValue == nullptr)
      os << "NULL";
    else
      os << var.payload.stringValue;
    break;
  case SHType::ContextVar:
    os << "ContextVariable: " << var.payload.stringValue;
    break;
  case SHType::Path:
    os << "Path: " << var.payload.stringValue;
    break;
  case SHType::Image:
    os << "Image";
    os << " Width: " << var.payload.imageValue.width;
    os << " Height: " << var.payload.imageValue.height;
    os << " Channels: " << (int)var.payload.imageValue.channels;
    break;
  case SHType::Audio:
    os << "Audio";
    os << " SampleRate: " << var.payload.audioValue.sampleRate;
    os << " Samples: " << var.payload.audioValue.nsamples;
    os << " Channels: " << var.payload.audioValue.channels;
    break;
  case SHType::Seq:
    os << "[";
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      const auto &v = var.payload.seqValue.elements[i];
      if (i == 0)
        os << v;
      else
        os << ", " << v;
    }
    os << "]";
    break;
  case Table: {
    os << "{";
    auto &t = var.payload.tableValue;
    bool first = true;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHString k;
    SHVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      if (first) {
        os << k << ": " << v;
        first = false;
      } else {
        os << ", " << k << ": " << v;
      }
    }
    os << "}";
  } break;
  case SHType::Set: {
    os << "{";
    auto &s = var.payload.setValue;
    bool first = true;
    SHSetIterator sit;
    s.api->setGetIterator(s, &sit);
    SHVar v;
    while (s.api->setNext(s, &sit, &v)) {
      if (first) {
        os << v;
        first = false;
      } else {
        os << ", " << v;
      }
    }
    os << "}";
  } break;
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const SHTypeInfo &t) {
  os << type2Name(t.basicType);
  if (t.basicType == SHType::Seq) {
    os << " [";
    for (uint32_t i = 0; i < t.seqTypes.len; i++) {
      // avoid recursive types
      if (t.seqTypes.elements[i].recursiveSelf) {
        os << "(Self)";
      } else {
        os << "(" << t.seqTypes.elements[i] << ")";
      }
      if (i < (t.seqTypes.len - 1)) {
        os << " ";
      }
    }
    os << "]";
  } else if (t.basicType == SHType::Set) {
    os << " [";
    for (uint32_t i = 0; i < t.setTypes.len; i++) {
      // avoid recursive types
      if (t.setTypes.elements[i].recursiveSelf) {
        os << "(Self)";
      } else {
        os << "(" << t.setTypes.elements[i] << ")";
      }
      if (i < (t.setTypes.len - 1)) {
        os << " ";
      }
    }
    os << "]";
  } else if (t.basicType == SHType::Table) {
    if (t.table.types.len == t.table.keys.len) {
      os << " {";
      for (uint32_t i = 0; i < t.table.types.len; i++) {
        os << "\"" << t.table.keys.elements[i] << "\" ";
        os << "(" << t.table.types.elements[i] << ")";
        if (i < (t.table.types.len - 1)) {
          os << " ";
        }
      }
      os << "}";
    } else {
      os << " [";
      for (uint32_t i = 0; i < t.table.types.len; i++) {
        if (t.table.types.elements[i].recursiveSelf) {
          os << "(Self)";
        } else {
          os << "(" << t.table.types.elements[i] << ")";
        }
        if (i < (t.table.types.len - 1)) {
          os << " ";
        }
      }
      os << "]";
    }
  } else if (t.basicType == SHType::ContextVar) {
    os << " [";
    for (uint32_t i = 0; i < t.contextVarTypes.len; i++) {
      // avoid recursive types
      if (t.contextVarTypes.elements[i].recursiveSelf) {
        os << "(Self)";
      } else {
        os << "(" << t.contextVarTypes.elements[i] << ")";
      }
      if (i < (t.contextVarTypes.len - 1)) {
        os << " ";
      }
    }
    os << "]";
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const SHTypesInfo &ts) {
  os << "[";
  for (uint32_t i = 0; i < ts.len; i++) {
    os << "(" << ts.elements[i] << ")";
    if (i < (ts.len - 1)) {
      os << " ";
    }
  }
  os << "]";
  return os;
}

bool _seqEq(const SHVar &a, const SHVar &b) {
  if (a.payload.seqValue.elements == b.payload.seqValue.elements)
    return true;

  if (a.payload.seqValue.len != b.payload.seqValue.len)
    return false;

  for (uint32_t i = 0; i < a.payload.seqValue.len; i++) {
    const auto &suba = a.payload.seqValue.elements[i];
    const auto &subb = b.payload.seqValue.elements[i];
    if (suba != subb)
      return false;
  }

  return true;
}

bool _setEq(const SHVar &a, const SHVar &b) {
  auto &ta = a.payload.setValue;
  auto &tb = b.payload.setValue;
  if (ta.opaque == tb.opaque)
    return true;

  if (ta.api->setSize(ta) != ta.api->setSize(tb))
    return false;

  SHSetIterator it;
  ta.api->setGetIterator(ta, &it);
  SHVar v;
  while (ta.api->setNext(ta, &it, &v)) {
    if (!tb.api->setContains(tb, v)) {
      return false;
    }
  }

  return true;
}

bool _tableEq(const SHVar &a, const SHVar &b) {
  auto &ta = a.payload.tableValue;
  auto &tb = b.payload.tableValue;
  if (ta.opaque == tb.opaque)
    return true;

  if (ta.api->tableSize(ta) != ta.api->tableSize(tb))
    return false;

  SHTableIterator it;
  ta.api->tableGetIterator(ta, &it);
  SHString k;
  SHVar v;
  while (ta.api->tableNext(ta, &it, &k, &v)) {
    if (!tb.api->tableContains(tb, k)) {
      return false;
    }
    const auto bval = tb.api->tableAt(tb, k);
    if (v != *bval) {
      return false;
    }
  }

  return true;
}

bool _seqLess(const SHVar &a, const SHVar &b) {
  auto alen = a.payload.seqValue.len;
  auto blen = b.payload.seqValue.len;
  auto len = std::min(alen, blen);

  for (uint32_t i = 0; i < len; i++) {
    auto c = cmp(a.payload.seqValue.elements[i], b.payload.seqValue.elements[i]);
    if (c < 0)
      return true;
    else if (c > 0)
      return false;
  }

  if (alen < blen)
    return true;
  else
    return false;
}

bool _tableLess(const SHVar &a, const SHVar &b) {
  auto &ta = a.payload.tableValue;
  auto &tb = b.payload.tableValue;
  if (ta.opaque == tb.opaque)
    return false;

  if (ta.api->tableSize(ta) != ta.api->tableSize(tb))
    return false;

  SHTableIterator it;
  ta.api->tableGetIterator(ta, &it);
  SHString k;
  SHVar v;
  size_t len = 0;
  while (ta.api->tableNext(ta, &it, &k, &v)) {
    if (!tb.api->tableContains(tb, k)) {
      return false;
    }
    const auto bval = tb.api->tableAt(tb, k);
    auto c = cmp(v, *bval);
    if (c < 0) {
      return true;
    } else if (c > 0) {
      return false;
    }
    len++;
  }

  if (ta.api->tableSize(ta) < len)
    return true;
  else
    return false;
}

bool _seqLessEq(const SHVar &a, const SHVar &b) {
  auto alen = a.payload.seqValue.len;
  auto blen = b.payload.seqValue.len;
  auto len = std::min(alen, blen);

  for (uint32_t i = 0; i < len; i++) {
    auto c = cmp(a.payload.seqValue.elements[i], b.payload.seqValue.elements[i]);
    if (c < 0)
      return true;
    else if (c > 0)
      return false;
  }

  if (alen <= blen)
    return true;
  else
    return false;
}

bool _tableLessEq(const SHVar &a, const SHVar &b) {
  auto &ta = a.payload.tableValue;
  auto &tb = b.payload.tableValue;
  if (ta.opaque == tb.opaque)
    return false;

  if (ta.api->tableSize(ta) != ta.api->tableSize(tb))
    return false;

  SHTableIterator it;
  ta.api->tableGetIterator(ta, &it);
  SHString k;
  SHVar v;
  size_t len = 0;
  while (ta.api->tableNext(ta, &it, &k, &v)) {
    if (!tb.api->tableContains(tb, k)) {
      return false;
    }
    const auto bval = tb.api->tableAt(tb, k);
    auto c = cmp(v, *bval);
    if (c < 0) {
      return true;
    } else if (c > 0) {
      return false;
    }
    len++;
  }

  if (ta.api->tableSize(ta) <= len)
    return true;
  else
    return false;
}

bool operator==(const SHTypeInfo &a, const SHTypeInfo &b) {
  if (a.basicType != b.basicType)
    return false;
  switch (a.basicType) {
  case SHType::Object:
    if (a.object.vendorId != b.object.vendorId)
      return false;
    return a.object.typeId == b.object.typeId;
  case SHType::Enum:
    if (a.enumeration.vendorId != b.enumeration.vendorId)
      return false;
    return a.enumeration.typeId == b.enumeration.typeId;
  case SHType::Seq: {
    if (a.seqTypes.elements == nullptr && b.seqTypes.elements == nullptr)
      return true;

    if (a.seqTypes.elements && b.seqTypes.elements) {
      if (a.seqTypes.len != b.seqTypes.len)
        return false;
      // compare but allow different orders of elements
      for (uint32_t i = 0; i < a.seqTypes.len; i++) {
        for (uint32_t j = 0; j < b.seqTypes.len; j++) {
          // consider recursive self a match
          if (a.seqTypes.elements[i].recursiveSelf == b.seqTypes.elements[j].recursiveSelf)
            goto matched_seq;
          if (a.seqTypes.elements[i] == b.seqTypes.elements[j])
            goto matched_seq;
        }
        return false;
      matched_seq:
        continue;
      }
    } else {
      return false;
    }

    return true;
  }
  case SHType::Set: {
    if (a.setTypes.elements == nullptr && b.setTypes.elements == nullptr)
      return true;

    if (a.setTypes.elements && b.setTypes.elements) {
      if (a.setTypes.len != b.setTypes.len)
        return false;
      // compare but allow different orders of elements
      for (uint32_t i = 0; i < a.setTypes.len; i++) {
        for (uint32_t j = 0; j < b.setTypes.len; j++) {
          // consider recursive self a match
          if (a.setTypes.elements[i].recursiveSelf == b.setTypes.elements[j].recursiveSelf)
            goto matched_set;
          if (a.setTypes.elements[i] == b.setTypes.elements[j])
            goto matched_set;
        }
        return false;
      matched_set:
        continue;
      }
    } else {
      return false;
    }

    return true;
  }
  case SHType::Table: {
    auto atypes = a.table.types.len;
    auto btypes = b.table.types.len;
    if (atypes != btypes)
      return false;

    auto akeys = a.table.keys.len;
    auto bkeys = b.table.keys.len;
    if (akeys != bkeys)
      return false;

    if (atypes != akeys && akeys != 0)
      return false;

    // compare but allow different orders of elements
    for (uint32_t i = 0; i < atypes; i++) {
      for (uint32_t j = 0; j < btypes; j++) {
        if (a.table.types.elements[i] == b.table.types.elements[j]) {
          if (a.table.keys.elements) { // this is enough to know they exist
            assert(i < akeys);
            if (!strcmp(a.table.keys.elements[i], b.table.keys.elements[j])) {
              goto matched_table;
            }
          } else {
            goto matched_table;
          }
        }
      }
      return false;
    matched_table:
      continue;
    }
    return true;
  }
  default:
    return true;
  }
}
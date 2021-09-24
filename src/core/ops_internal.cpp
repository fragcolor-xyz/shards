/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "ops_internal.hpp"
#include "ops.hpp"
#include <unordered_set>

std::ostream &operator<<(std::ostream &os, const CBVar &var) {
  switch (var.valueType) {
  case CBType::EndOfBlittableTypes:
    break;
  case CBType::None:
    os << "None";
    break;
  case CBType::Any:
    os << "Any";
    break;
  case CBType::Object:
    os << "Object: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.objectValue) << " vendor: 0x"
       << var.payload.objectVendorId << " type: 0x" << var.payload.objectTypeId
       << std::dec;
    break;
  case CBType::Chain:
    os << "Chain: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.chainValue) << std::dec;
    break;
  case CBType::Bytes:
    os << "Bytes: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.bytesValue)
       << " size: " << std::dec << var.payload.bytesSize;
    break;
  case CBType::Array:
    os << "Array: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.arrayValue.elements)
       << " size: " << std::dec << var.payload.arrayValue.len
       << " of: " << type2Name(var.innerType);
    break;
  case CBType::Enum:
    os << "Enum: " << var.payload.enumValue << std::hex << " vendor: 0x"
       << var.payload.enumVendorId << " type: 0x" << var.payload.enumTypeId
       << std::dec;
    break;
  case CBType::Bool:
    os << (var.payload.boolValue ? "true" : "false");
    break;
  case CBType::Int:
    os << var.payload.intValue;
    break;
  case CBType::Int2:
    os << "(";
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.int2Value[i];
      else
        os << ", " << var.payload.int2Value[i];
    }
    os << ")";
    break;
  case CBType::Int3:
    os << "(";
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.int3Value[i];
      else
        os << ", " << var.payload.int3Value[i];
    }
    os << ")";
    break;
  case CBType::Int4:
    os << "(";
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.int4Value[i];
      else
        os << ", " << var.payload.int4Value[i];
    }
    os << ")";
    break;
  case CBType::Int8:
    os << "(";
    for (auto i = 0; i < 8; i++) {
      if (i == 0)
        os << var.payload.int8Value[i];
      else
        os << ", " << var.payload.int8Value[i];
    }
    os << ")";
    break;
  case CBType::Int16:
    os << "(";
    for (auto i = 0; i < 16; i++) {
      if (i == 0)
        os << int(var.payload.int16Value[i]);
      else
        os << ", " << int(var.payload.int16Value[i]);
    }
    os << ")";
    break;
  case CBType::Float:
    os << var.payload.floatValue;
    break;
  case CBType::Float2:
    os << "(";
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.float2Value[i];
      else
        os << ", " << var.payload.float2Value[i];
    }
    os << ")";
    break;
  case CBType::Float3:
    os << "(";
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.float3Value[i];
      else
        os << ", " << var.payload.float3Value[i];
    }
    os << ")";
    break;
  case CBType::Float4:
    os << "(";
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.float4Value[i];
      else
        os << ", " << var.payload.float4Value[i];
    }
    os << ")";
    break;
  case CBType::Color:
    os << int(var.payload.colorValue.r) << ", " << int(var.payload.colorValue.g)
       << ", " << int(var.payload.colorValue.b) << ", "
       << int(var.payload.colorValue.a);
    break;
  case CBType::Block:
    os << "Block: " << var.payload.blockValue->name(var.payload.blockValue);
    break;
  case CBType::String:
    if (var.payload.stringValue == nullptr)
      os << "NULL";
    else
      os << "\"" << var.payload.stringValue << "\"";
    break;
  case CBType::ContextVar:
    os << "ContextVariable: " << var.payload.stringValue;
    break;
  case CBType::Path:
    os << "Path: " << var.payload.stringValue;
    break;
  case CBType::Image:
    os << "Image";
    os << " Width: " << var.payload.imageValue.width;
    os << " Height: " << var.payload.imageValue.height;
    os << " Channels: " << (int)var.payload.imageValue.channels;
    break;
  case CBType::Audio:
    os << "Audio";
    os << " SampleRate: " << var.payload.audioValue.sampleRate;
    os << " Samples: " << var.payload.audioValue.nsamples;
    os << " Channels: " << var.payload.audioValue.channels;
    break;
  case CBType::Seq:
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
    CBTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    CBString k;
    CBVar v;
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
  case CBType::Set: {
    os << "{";
    auto &s = var.payload.setValue;
    bool first = true;
    CBSetIterator sit;
    s.api->setGetIterator(s, &sit);
    CBVar v;
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

std::ostream &operator<<(std::ostream &os, const CBTypeInfo &t) {
  os << type2Name(t.basicType);
  if (t.basicType == CBType::Seq) {
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
  } else if (t.basicType == CBType::Set) {
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
  } else if (t.basicType == CBType::Table) {
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
  } else if (t.basicType == CBType::ContextVar) {
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

std::ostream &operator<<(std::ostream &os, const CBTypesInfo &ts) {
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

bool _seqEq(const CBVar &a, const CBVar &b) {
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

bool _setEq(const CBVar &a, const CBVar &b) {
  auto &ta = a.payload.setValue;
  auto &tb = b.payload.setValue;
  if (ta.opaque == tb.opaque)
    return true;

  if (ta.api->setSize(ta) != ta.api->setSize(tb))
    return false;

  CBSetIterator it;
  ta.api->setGetIterator(ta, &it);
  CBVar v;
  while (ta.api->setNext(ta, &it, &v)) {
    if (!tb.api->setContains(tb, v)) {
      return false;
    }
  }

  return true;
}

bool _tableEq(const CBVar &a, const CBVar &b) {
  auto &ta = a.payload.tableValue;
  auto &tb = b.payload.tableValue;
  if (ta.opaque == tb.opaque)
    return true;

  if (ta.api->tableSize(ta) != ta.api->tableSize(tb))
    return false;

  CBTableIterator it;
  ta.api->tableGetIterator(ta, &it);
  CBString k;
  CBVar v;
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

bool _seqLess(const CBVar &a, const CBVar &b) {
  auto alen = a.payload.seqValue.len;
  auto blen = b.payload.seqValue.len;
  auto len = std::min(alen, blen);

  for (uint32_t i = 0; i < len; i++) {
    auto c =
        cmp(a.payload.seqValue.elements[i], b.payload.seqValue.elements[i]);
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

bool _tableLess(const CBVar &a, const CBVar &b) {
  auto &ta = a.payload.tableValue;
  auto &tb = b.payload.tableValue;
  if (ta.opaque == tb.opaque)
    return false;

  if (ta.api->tableSize(ta) != ta.api->tableSize(tb))
    return false;

  CBTableIterator it;
  ta.api->tableGetIterator(ta, &it);
  CBString k;
  CBVar v;
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

bool _seqLessEq(const CBVar &a, const CBVar &b) {
  auto alen = a.payload.seqValue.len;
  auto blen = b.payload.seqValue.len;
  auto len = std::min(alen, blen);

  for (uint32_t i = 0; i < len; i++) {
    auto c =
        cmp(a.payload.seqValue.elements[i], b.payload.seqValue.elements[i]);
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

bool _tableLessEq(const CBVar &a, const CBVar &b) {
  auto &ta = a.payload.tableValue;
  auto &tb = b.payload.tableValue;
  if (ta.opaque == tb.opaque)
    return false;

  if (ta.api->tableSize(ta) != ta.api->tableSize(tb))
    return false;

  CBTableIterator it;
  ta.api->tableGetIterator(ta, &it);
  CBString k;
  CBVar v;
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

bool operator==(const CBTypeInfo &a, const CBTypeInfo &b) {
  if (a.basicType != b.basicType)
    return false;
  switch (a.basicType) {
  case CBType::Object:
    if (a.object.vendorId != b.object.vendorId)
      return false;
    return a.object.typeId == b.object.typeId;
  case CBType::Enum:
    if (a.enumeration.vendorId != b.enumeration.vendorId)
      return false;
    return a.enumeration.typeId == b.enumeration.typeId;
  case CBType::Seq: {
    if (a.seqTypes.elements == nullptr && b.seqTypes.elements == nullptr)
      return true;

    if (a.seqTypes.elements && b.seqTypes.elements) {
      if (a.seqTypes.len != b.seqTypes.len)
        return false;
      // compare but allow different orders of elements
      for (uint32_t i = 0; i < a.seqTypes.len; i++) {
        for (uint32_t j = 0; j < b.seqTypes.len; j++) {
          // consider recursive self a match
          if (a.seqTypes.elements[i].recursiveSelf ==
              b.seqTypes.elements[j].recursiveSelf)
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
  case CBType::Set: {
    if (a.setTypes.elements == nullptr && b.setTypes.elements == nullptr)
      return true;

    if (a.setTypes.elements && b.setTypes.elements) {
      if (a.setTypes.len != b.setTypes.len)
        return false;
      // compare but allow different orders of elements
      for (uint32_t i = 0; i < a.setTypes.len; i++) {
        for (uint32_t j = 0; j < b.setTypes.len; j++) {
          // consider recursive self a match
          if (a.setTypes.elements[i].recursiveSelf ==
              b.setTypes.elements[j].recursiveSelf)
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
  case CBType::Table: {
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
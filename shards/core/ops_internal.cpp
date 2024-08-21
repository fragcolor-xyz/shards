/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "ops_internal.hpp"
#include <shards/ops.hpp>
#include "runtime.hpp"
#include <unordered_set>

namespace shards {
std::ostream &DocsFriendlyFormatter::format(std::ostream &os, const SHVar &var) {
  switch (var.valueType) {
  case SHType::Type:
    os << "@type(";
    format(os, *var.payload.typeValue);
    os << ")";
    break;
  case SHType::None:
    os << "None";
    break;
  case SHType::Any:
    os << "Any";
    break;
  case SHType::Object: {
    const SHObjectInfo *objectInfo = var.objectInfo;
    if (!objectInfo)
      objectInfo = shards::findObjectInfo(var.payload.objectVendorId, var.payload.objectTypeId);
    if (objectInfo) {
      os << objectInfo->name;
    } else {
      os << "Object: 0x" << std::hex << var.payload.objectValue << " vendor: 0x" << var.payload.objectVendorId << " type: 0x"
         << var.payload.objectTypeId << std::dec;
    }
    break;
  }
  case SHType::Wire: {
    if (var.payload.wireValue) {
      auto wire = SHWire::sharedFromRef(var.payload.wireValue);
      os << "<Wire: " << wire->name << ">";
    } else {
      os << "<Wire: None>";
    }
  } break;
  case SHType::Bytes:
    os << "<" << var.payload.bytesSize << " SHType::Bytes>" << std::dec;
    break;
  case SHType::Array:
    os << "Array: 0x" << std::hex << reinterpret_cast<uintptr_t>(var.payload.arrayValue.elements) << " size: " << std::dec
       << var.payload.arrayValue.len << " of: " << type2Name(var.innerType);
    break;
  case SHType::Enum: {
    const SHEnumInfo *enumInfo = findEnumInfo(var.payload.enumVendorId, var.payload.enumTypeId);
    if (enumInfo) {
      const char *label = "<invalid>";
      if (var.payload.enumValue >= 0 && var.payload.enumValue < SHEnum(enumInfo->labels.len)) {
        label = enumInfo->labels.elements[var.payload.enumValue];
      }
      os << enumInfo->name << "::" << label;
    } else {
      os << "Enum: " << var.payload.enumValue << std::hex << " vendor: 0x" << var.payload.enumVendorId << " type: 0x"
         << var.payload.enumTypeId << std::dec;
    }
    break;
  }
  case SHType::Bool:
    os << (var.payload.boolValue ? "true" : "false");
    break;
  case SHType::Int:
    os << var.payload.intValue;
    break;
  case SHType::Int2:
    os << "@i2(";
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.int2Value[i];
      else
        os << " " << var.payload.int2Value[i];
    }
    os << ")";
    break;
  case SHType::Int3:
    os << "@i3(";
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.int3Value[i];
      else
        os << " " << var.payload.int3Value[i];
    }
    os << ")";
    break;
  case SHType::Int4:
    os << "@i4(";
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.int4Value[i];
      else
        os << " " << var.payload.int4Value[i];
    }
    os << ")";
    break;
  case SHType::Int8:
    os << "@i8(";
    for (auto i = 0; i < 8; i++) {
      if (i == 0)
        os << var.payload.int8Value[i];
      else
        os << " " << var.payload.int8Value[i];
    }
    os << ")";
    break;
  case SHType::Int16:
    os << "@i16(";
    for (auto i = 0; i < 16; i++) {
      if (i == 0)
        os << int(var.payload.int16Value[i]);
      else
        os << " " << int(var.payload.int16Value[i]);
    }
    os << ")";
    break;
  case SHType::Float:
    os << var.payload.floatValue;
    break;
  case SHType::Float2:
    os << "@f2(";
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.float2Value[i];
      else
        os << " " << var.payload.float2Value[i];
    }
    os << ")";
    break;
  case SHType::Float3:
    os << "@f3(";
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.float3Value[i];
      else
        os << " " << var.payload.float3Value[i];
    }
    os << ")";
    break;
  case SHType::Float4:
    os << "@f4(";
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.float4Value[i];
      else
        os << " " << var.payload.float4Value[i];
    }
    os << ")";
    break;
  case SHType::Color:
    os << "@color(" << int(var.payload.colorValue.r) << " " << int(var.payload.colorValue.g) << " "
       << int(var.payload.colorValue.b) << " " << int(var.payload.colorValue.a) << ")";
    break;
  case SHType::ShardRef:
    os << "Shard: " << var.payload.shardValue->name(var.payload.shardValue);
    break;
  case SHType::String: {
    auto sView = SHSTRVIEW(var);
    os << sView;
  } break;
  case SHType::ContextVar: {
    auto sView = SHSTRVIEW(var);
    os << "ContextVariable: " << sView;
  } break;
  case SHType::Path: {
    auto sView = SHSTRVIEW(var);
    os << "Path: " << sView;
  } break;
  case SHType::Image:
    os << "Image(" << std::hex << var.payload.imageValue << std::dec << ")";
    os << " Width: " << var.payload.imageValue->width;
    os << " Height: " << var.payload.imageValue->height;
    os << " Channels: " << (int)var.payload.imageValue->channels;
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
        os << " " << v;
    }
    os << "]";
    break;
  case SHType::Table: {
    os << "{";
    auto &t = var.payload.tableValue;
    bool first = true;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHVar k;
    SHVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      if (first) {
        os << k << ": " << v;
        first = false;
      } else {
        os << " " << k << ": " << v;
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
        os << " " << v;
      }
    }
    os << "}";
  } break;
  case SHType::Trait: {
    auto &t = *var.payload.traitValue;
    os << t;
  } break;
  default:
    shassert(false && "Invalid variable type");
    break;
  }
  return os;
}

std::ostream &DocsFriendlyFormatter::format(std::ostream &os, const SHTrait &t) {
  return os << "Trait " << t.name << " / 0x" << std::hex << t.id[1] << t.id[0];
}

std::ostream &DocsFriendlyFormatter::format(std::ostream &os, const SHTypeInfo &t) {
  // This code outputs non-breaking spaces for cleaner wrapping on the documentation page
  if (t.basicType == SHType::Seq) {
    os << "[";

    for (uint32_t i = 0; i < t.seqTypes.len; i++) {
      if (t.seqTypes.elements[i].recursiveSelf) {
        os << "Self";
      } else {
        os << t.seqTypes.elements[i];
      }
      if (i < (t.seqTypes.len - 1)) {
        os << " ";
      }
    }
    os << "]";

    if (t.fixedSize != 0) {
      os << "(" << t.fixedSize << ")";
    }
  } else if (t.basicType == SHType::Set) {
    os << "<";
    for (uint32_t i = 0; i < t.setTypes.len; i++) {
      // avoid recursive types
      if (t.setTypes.elements[i].recursiveSelf) {
        os << "Self";
      } else {
        os << t.setTypes.elements[i];
      }
      if (i < (t.setTypes.len - 1)) {
        os << " ";
      }
    }
    os << ">";
  } else if (t.basicType == SHType::Table) {
    if (t.table.types.len == t.table.keys.len) {
      os << "{";
      for (uint32_t i = 0; i < t.table.types.len; i++) {
        os << t.table.keys.elements[i] << ":"
           << " ";
        os << t.table.types.elements[i];
        if (i < (t.table.types.len - 1)) {
          os << " ";
        }
      }
      os << "}";
    } else {
      os << "{";
      for (uint32_t i = 0; i < t.table.types.len; i++) {
        if (t.table.types.elements[i].recursiveSelf) {
          os << "Self";
        } else {
          os << t.table.types.elements[i];
        }
        if (i < (t.table.types.len - 1)) {
          os << " ";
        }
      }
      os << "}";
    }
  } else if (t.basicType == SHType::ContextVar) {
    os << "Var(";

    if (t.contextVarTypes.len == 0) {
      os << "Any";
    }

    for (uint32_t i = 0; i < t.contextVarTypes.len; i++) {
      // avoid recursive types
      if (t.contextVarTypes.elements[i].recursiveSelf) {
        os << "Self";
      } else {
        os << t.contextVarTypes.elements[i];
      }
      if (i < (t.contextVarTypes.len - 1)) {
        os << " ";
      }
    }

    os << ")";
  } else if (t.basicType == SHType::Object) {
    const SHObjectInfo *objectInfo = findObjectInfo(t.object.vendorId, t.object.typeId);
    if (objectInfo) {
      os << objectInfo->name;
    } else {
      os << "Object";
      SHLOG_WARNING("No object info found for object: vendor: {}, type: {}", t.object.vendorId, t.object.typeId);
    }
  } else if (t.basicType == SHType::Enum) {
    const SHEnumInfo *enumInfo = findEnumInfo(t.enumeration.vendorId, t.enumeration.typeId);
    if (enumInfo) {
      os << enumInfo->name;
    } else {
      os << "Enum";
      SHLOG_WARNING("No object info found for enum: vendor: {}, type: {}", t.enumeration.vendorId, t.enumeration.typeId);
    }
  } else {
    os << type2Name(t.basicType);
  }
  return os;
}

std::ostream &DocsFriendlyFormatter::format(std::ostream &os, const SHTypesInfo &ts) {
  for (uint32_t i = 0; i < ts.len; i++) {
    if (ignoreNone && ts.elements[i].basicType == SHType::None)
      continue;
    os << ts.elements[i];
    if (i < (ts.len - 1)) {
      os << " ";
    }
  }
  return os;
}

std::ostream &DocsFriendlyFormatter::format(std::ostream &os, const SHStringWithLen &s) {
  std::string_view sv(s.string, size_t(s.len));
  os << sv;
  return os;
}
} // namespace shards

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
  SHVar k;
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
  SHVar k;
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
  SHVar k;
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
          if (a.seqTypes.elements[i].recursiveSelf &&
              a.seqTypes.elements[i].recursiveSelf == b.seqTypes.elements[j].recursiveSelf)
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
            shassert(i < akeys);
            if (a.table.keys.elements[i] == b.table.keys.elements[j]) {
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

ALWAYS_INLINE inline bool _almostEqual(const float a, const float b, const double e) { return __builtin_fabsf(a - b) <= e; }
ALWAYS_INLINE inline bool _almostEqual(const double a, const double b, const double e) { return __builtin_fabs(a - b) <= e; }
ALWAYS_INLINE inline bool _almostEqual(const int32_t a, const int32_t b, const double e) { return __builtin_abs(a - b) <= e; }
ALWAYS_INLINE inline bool _almostEqual(const int64_t a, const int64_t b, const double e) { return __builtin_llabs(a - b) <= e; }

bool _almostEqual(const SHVar &lhs, const SHVar &rhs, double e) {
  if (lhs.valueType != rhs.valueType) {
    return false;
  }

  switch (lhs.valueType) {
  case SHType::Float:
    return _almostEqual(lhs.payload.floatValue, rhs.payload.floatValue, e);
  case SHType::Float2:
    return (_almostEqual(lhs.payload.float2Value[0], rhs.payload.float2Value[0], e) &&
            _almostEqual(lhs.payload.float2Value[1], rhs.payload.float2Value[1], e));
  case SHType::Float3:
    return (_almostEqual(lhs.payload.float3Value[0], rhs.payload.float3Value[0], e) &&
            _almostEqual(lhs.payload.float3Value[1], rhs.payload.float3Value[1], e) &&
            _almostEqual(lhs.payload.float3Value[2], rhs.payload.float3Value[2], e));
  case SHType::Float4:
    return (_almostEqual(lhs.payload.float4Value[0], rhs.payload.float4Value[0], e) &&
            _almostEqual(lhs.payload.float4Value[1], rhs.payload.float4Value[1], e) &&
            _almostEqual(lhs.payload.float4Value[2], rhs.payload.float4Value[2], e) &&
            _almostEqual(lhs.payload.float4Value[3], rhs.payload.float4Value[3], e));
  case SHType::Int:
    return _almostEqual(lhs.payload.intValue, rhs.payload.intValue, e);
  case SHType::Int2:
    return (_almostEqual(lhs.payload.int2Value[0], rhs.payload.int2Value[0], e) &&
            _almostEqual(lhs.payload.int2Value[1], rhs.payload.int2Value[1], e));
  case SHType::Int3:
    return (_almostEqual(lhs.payload.int3Value[0], rhs.payload.int3Value[0], e) &&
            _almostEqual(lhs.payload.int3Value[1], rhs.payload.int3Value[1], e) &&
            _almostEqual(lhs.payload.int3Value[2], rhs.payload.int3Value[2], e));
  case SHType::Int4:
    return (_almostEqual(lhs.payload.int4Value[0], rhs.payload.int4Value[0], e) &&
            _almostEqual(lhs.payload.int4Value[1], rhs.payload.int4Value[1], e) &&
            _almostEqual(lhs.payload.int4Value[2], rhs.payload.int4Value[2], e) &&
            _almostEqual(lhs.payload.int4Value[3], rhs.payload.int4Value[3], e));
  case SHType::Int8:
    return (_almostEqual(lhs.payload.int8Value[0], rhs.payload.int8Value[0], e) &&
            _almostEqual(lhs.payload.int8Value[1], rhs.payload.int8Value[1], e) &&
            _almostEqual(lhs.payload.int8Value[2], rhs.payload.int8Value[2], e) &&
            _almostEqual(lhs.payload.int8Value[3], rhs.payload.int8Value[3], e) &&
            _almostEqual(lhs.payload.int8Value[4], rhs.payload.int8Value[4], e) &&
            _almostEqual(lhs.payload.int8Value[5], rhs.payload.int8Value[5], e) &&
            _almostEqual(lhs.payload.int8Value[6], rhs.payload.int8Value[6], e) &&
            _almostEqual(lhs.payload.int8Value[7], rhs.payload.int8Value[7], e));
  case SHType::Int16:
    return (_almostEqual(lhs.payload.int16Value[0], rhs.payload.int16Value[0], e) &&
            _almostEqual(lhs.payload.int16Value[1], rhs.payload.int16Value[1], e) &&
            _almostEqual(lhs.payload.int16Value[2], rhs.payload.int16Value[2], e) &&
            _almostEqual(lhs.payload.int16Value[3], rhs.payload.int16Value[3], e) &&
            _almostEqual(lhs.payload.int16Value[4], rhs.payload.int16Value[4], e) &&
            _almostEqual(lhs.payload.int16Value[5], rhs.payload.int16Value[5], e) &&
            _almostEqual(lhs.payload.int16Value[6], rhs.payload.int16Value[6], e) &&
            _almostEqual(lhs.payload.int16Value[7], rhs.payload.int16Value[7], e) &&
            _almostEqual(lhs.payload.int16Value[8], rhs.payload.int16Value[8], e) &&
            _almostEqual(lhs.payload.int16Value[9], rhs.payload.int16Value[9], e) &&
            _almostEqual(lhs.payload.int16Value[10], rhs.payload.int16Value[10], e) &&
            _almostEqual(lhs.payload.int16Value[11], rhs.payload.int16Value[11], e) &&
            _almostEqual(lhs.payload.int16Value[12], rhs.payload.int16Value[12], e) &&
            _almostEqual(lhs.payload.int16Value[13], rhs.payload.int16Value[13], e) &&
            _almostEqual(lhs.payload.int16Value[14], rhs.payload.int16Value[14], e) &&
            _almostEqual(lhs.payload.int16Value[15], rhs.payload.int16Value[15], e));
  case SHType::Seq: {
    if (lhs.payload.seqValue.len != rhs.payload.seqValue.len) {
      return false;
    }

    auto almost = true;
    for (uint32_t i = 0; i < lhs.payload.seqValue.len; i++) {
      auto &suba = lhs.payload.seqValue.elements[i];
      auto &subb = rhs.payload.seqValue.elements[i];
      almost = almost && _almostEqual(suba, subb, e);
    }

    return almost;
  }
  default:
    return lhs == rhs;
  }
}

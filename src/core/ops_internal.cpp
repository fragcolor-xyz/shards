#include "ops_internal.hpp"
#include "ops.hpp"
#include <unordered_set>

MAKE_LOGGABLE(CBVar, var, os) {
  switch (var.valueType) {
  case EndOfBlittableTypes:
    break;
  case None:
    os << "*None*";
    break;
  case CBType::Any:
    os << "*Any*";
    break;
  case Object:
    os << "Object: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.objectValue) << std::dec;
    break;
  case Chain:
    os << "Chain: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.chainValue) << std::dec;
    break;
  case Bytes:
    os << "Bytes: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.bytesValue)
       << " size: " << std::dec << var.payload.bytesSize;
    break;
  case Array:
    os << "Array: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.arrayValue.elements)
       << " size: " << std::dec << var.payload.arrayValue.len
       << " of: " << type2Name(var.innerType);
    break;
  case Enum:
    os << "Enum: " << var.payload.enumValue;
    break;
  case Bool:
    os << (var.payload.boolValue ? "true" : "false");
    break;
  case Int:
    os << var.payload.intValue;
    break;
  case Int2:
    os << "(";
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.int2Value[i];
      else
        os << ", " << var.payload.int2Value[i];
    }
    os << ")";
    break;
  case Int3:
    os << "(";
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.int3Value[i];
      else
        os << ", " << var.payload.int3Value[i];
    }
    os << ")";
    break;
  case Int4:
    os << "(";
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.int4Value[i];
      else
        os << ", " << var.payload.int4Value[i];
    }
    os << ")";
    break;
  case Int8:
    os << "(";
    for (auto i = 0; i < 8; i++) {
      if (i == 0)
        os << var.payload.int8Value[i];
      else
        os << ", " << var.payload.int8Value[i];
    }
    os << ")";
    break;
  case Int16:
    os << "(";
    for (auto i = 0; i < 16; i++) {
      if (i == 0)
        os << int(var.payload.int16Value[i]);
      else
        os << ", " << int(var.payload.int16Value[i]);
    }
    os << ")";
    break;
  case Float:
    os << var.payload.floatValue;
    break;
  case Float2:
    os << "(";
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.float2Value[i];
      else
        os << ", " << var.payload.float2Value[i];
    }
    os << ")";
    break;
  case Float3:
    os << "(";
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.float3Value[i];
      else
        os << ", " << var.payload.float3Value[i];
    }
    os << ")";
    break;
  case Float4:
    os << "(";
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.float4Value[i];
      else
        os << ", " << var.payload.float4Value[i];
    }
    os << ")";
    break;
  case Color:
    os << int(var.payload.colorValue.r) << ", " << int(var.payload.colorValue.g)
       << ", " << int(var.payload.colorValue.b) << ", "
       << int(var.payload.colorValue.a);
    break;
  case Block:
    os << "Block: " << var.payload.blockValue->name(var.payload.blockValue);
    break;
  case CBType::String:
    os << var.payload.stringValue;
    break;
  case ContextVar:
    os << "ContextVariable: " << var.payload.stringValue;
    break;
  case CBType::Path:
    os << "Path: " << var.payload.stringValue;
    break;
  case Image:
    os << "Image";
    os << " Width: " << var.payload.imageValue.width;
    os << " Height: " << var.payload.imageValue.height;
    os << " Channels: " << (int)var.payload.imageValue.channels;
    break;
  case Seq:
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
  case Table:
    os << "{";
    auto &ta = var.payload.tableValue;
    struct iterdata {
      bool first;
      el::base::type::ostream_t *os;
    } data;
    data.first = true;
    data.os = &os;
    ta.api->tableForEach(
        ta,
        [](const char *key, CBVar *value, void *_data) {
          auto data = (iterdata *)_data;
          if (data->first) {
            *data->os << key << ": " << *value;
            data->first = false;
          } else {
            *data->os << ", " << key << ": " << *value;
          }
          return true;
        },
        &data);
    os << "}";
    break;
  }
  return os;
}

MAKE_LOGGABLE(CBTypeInfo, t, os) {
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
  }
  return os;
}

MAKE_LOGGABLE(CBTypesInfo, ts, os) {
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
    auto &bval = *tb.api->tableAt(tb, k);
    if (v != bval) {
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
    auto &bval = *tb.api->tableAt(tb, k);
    auto c = cmp(v, bval);
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
    auto &bval = *tb.api->tableAt(tb, k);
    auto c = cmp(v, bval);
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
  case Object:
    if (a.object.vendorId != b.object.vendorId)
      return false;
    return a.object.typeId == b.object.typeId;
  case Enum:
    if (a.enumeration.vendorId != b.enumeration.vendorId)
      return false;
    return a.enumeration.typeId == b.enumeration.typeId;
  case Seq: {
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
  case Table: {
    auto atypes = a.table.types.len;
    auto btypes = b.table.types.len;
    if (atypes != btypes)
      return false;

    auto akeys = a.table.keys.len;
    auto bkeys = b.table.keys.len;
    if (akeys != bkeys)
      return false;

    // compare but allow different orders of elements
    for (uint32_t i = 0; i < atypes; i++) {
      for (uint32_t j = 0; j < btypes; j++) {
        if (a.table.types.elements[i] == b.table.types.elements[j]) {
          if (a.table.keys.elements) { // this is enough to know they exist
            if (strcmp(a.table.keys.elements[i], b.table.keys.elements[j]) ==
                0) {
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
  return true;
}
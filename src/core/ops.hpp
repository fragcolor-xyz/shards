/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_OPS
#define CB_OPS

#include <cfloat>
#include <chainblocks.h>
#include <easylogging++.h>
#include <stb_ds.h>

inline std::string type2Name(CBType type) {
  std::string name = "";
  switch (type) {
  case EndOfBlittableTypes:
    break;
  case None:
    name = "None";
    break;
  case CBType::Any:
    name = "Any";
    break;
  case Object:
    name = "Object";
    break;
  case Enum:
    name = "Enum";
    break;
  case Bool:
    name = "Bool";
    break;
  case Bytes:
    name = "Bytes";
    break;
  case Int:
    name = "Int";
    break;
  case Int2:
    name = "Int2";
    break;
  case Int3:
    name = "Int3";
    break;
  case Int4:
    name = "Int4";
    break;
  case Int8:
    name = "Int8";
    break;
  case Int16:
    name = "Int16";
    break;
  case Float:
    name = "Float";
    break;
  case Float2:
    name = "Float2";
    break;
  case Float3:
    name = "Float3";
    break;
  case Float4:
    name = "Float4";
    break;
  case Color:
    name = "Color";
    break;
  case Chain:
    name = "Chain";
    break;
  case Block:
    name = "Block";
    break;
  case CBType::String:
    name = "String";
    break;
  case ContextVar:
    name = "ContextVar";
    break;
  case Image:
    name = "Image";
    break;
  case Seq:
    name = "Seq";
    break;
  case Table:
    name = "Table";
    break;
  case Vector:
    name = "Vector";
    break;
  case List:
    name = "List";
    break;
  case Node:
    name = "Node";
    break;
  case CBType::TypeInfo:
    name = "Type";
    break;
  }
  return name;
}

inline MAKE_LOGGABLE(CBVar, var, os) {
  switch (var.valueType) {
  case EndOfBlittableTypes:
    break;
  case None:
    if (var.payload.chainState == Continue)
      os << "*None*";
    else if (var.payload.chainState == CBChainState::Stop)
      os << "*ChainStop*";
    else if (var.payload.chainState == CBChainState::Restart)
      os << "*ChainRestart*";
    else if (var.payload.chainState == CBChainState::Return)
      os << "*ChainReturn*";
    else if (var.payload.chainState == CBChainState::Rebase)
      os << "*ChainRebase*";
    break;
  case CBType::TypeInfo:
    if (var.payload.typeInfoValue)
      os << type2Name(var.payload.typeInfoValue->basicType);
    else
      os << "None";
    break;
  case CBType::Any:
    os << "*Any*";
    break;
  case Object:
    os << "Object: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.objectValue) << std::dec;
    break;
  case Node:
    os << "Node: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.nodeValue) << std::dec;
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
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.int2Value[i];
      else
        os << ", " << var.payload.int2Value[i];
    }
    break;
  case Int3:
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.int3Value[i];
      else
        os << ", " << var.payload.int3Value[i];
    }
    break;
  case Int4:
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.int4Value[i];
      else
        os << ", " << var.payload.int4Value[i];
    }
    break;
  case Int8:
    for (auto i = 0; i < 8; i++) {
      if (i == 0)
        os << var.payload.int8Value[i];
      else
        os << ", " << var.payload.int8Value[i];
    }
    break;
  case Int16:
    for (auto i = 0; i < 16; i++) {
      if (i == 0)
        os << var.payload.int16Value[i];
      else
        os << ", " << var.payload.int16Value[i];
    }
    break;
  case Float:
    os << var.payload.floatValue;
    break;
  case Float2:
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.float2Value[i];
      else
        os << ", " << var.payload.float2Value[i];
    }
    break;
  case Float3:
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.float3Value[i];
      else
        os << ", " << var.payload.float3Value[i];
    }
    break;
  case Float4:
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.float4Value[i];
      else
        os << ", " << var.payload.float4Value[i];
    }
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
    os << "*ContextVariable*: " << var.payload.stringValue;
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
      if (i == 0)
        os << var.payload.seqValue.elements[i];
      else
        os << ", " << var.payload.seqValue.elements[i];
    }
    os << "]";
    break;
  case Table:
    os << "{";
    for (ptrdiff_t i = 0; i < stbds_shlen(var.payload.tableValue); i++) {
      if (i == 0)
        os << var.payload.tableValue[i].key << ": "
           << var.payload.tableValue[i].value;
      else
        os << ", " << var.payload.tableValue[i].key << ": "
           << var.payload.tableValue[i].value;
    }
    os << "}";
    break;
  case Vector: {
    os << "[";
    CBVar vv{};
    vv.valueType = var.payload.vectorType;
    for (uint32_t i = 0; i < var.payload.vectorSize; i++) {
      vv.payload = var.payload.vectorValue[i];
      if (i == 0)
        os << vv;
      else
        os << ", " << vv;
    }
    os << "]";
  } break;
  case List: {
    os << "(";
    auto next = var.payload.listValue.value;
    while (next) {
      if (var.payload.listValue.next) {
        os << next << " ";
        next = var.payload.listValue.next;
      } else {
        os << *next;
      }
    }
    os << ")";
  } break;
  }
  return os;
}

ALWAYS_INLINE inline bool operator!=(const CBVar &a, const CBVar &b);
ALWAYS_INLINE inline bool operator<(const CBVar &a, const CBVar &b);
ALWAYS_INLINE inline bool operator>(const CBVar &a, const CBVar &b);
ALWAYS_INLINE inline bool operator>=(const CBVar &a, const CBVar &b);
ALWAYS_INLINE inline bool operator==(const CBVar &a, const CBVar &b);

inline bool operator==(const CBTypeInfo &a, const CBTypeInfo &b);
inline bool operator!=(const CBTypeInfo &a, const CBTypeInfo &b);

inline int cmp(const CBVar &a, const CBVar &b) {
  if (a == b)
    return 0;
  else if (a < b)
    return -1;
  else
    return 1;
}

inline bool _seqEq(const CBVar &a, const CBVar &b) {
  if (a.payload.seqValue.elements == b.payload.seqValue.elements)
    return true;

  if (a.payload.seqValue.len != b.payload.seqValue.len)
    return false;

  for (uint32_t i = 0; i < a.payload.seqValue.len; i++) {
    if (!(a.payload.seqValue.elements[i] == b.payload.seqValue.elements[i]))
      return false;
  }

  return true;
}

inline bool _tableEq(const CBVar &a, const CBVar &b) {
  if (a.payload.tableValue == b.payload.tableValue)
    return true;

  if (stbds_shlen(a.payload.tableValue) != stbds_shlen(b.payload.tableValue))
    return false;

  for (ptrdiff_t i = 0; i < stbds_shlen(a.payload.tableValue); i++) {
    if (strcmp(a.payload.tableValue[i].key, b.payload.tableValue[i].key) != 0)
      return false;

    if (!(a.payload.tableValue[i].value == b.payload.tableValue[i].value))
      return false;
  }

  return true;
}

inline bool _vectorEq(const CBVar &a, const CBVar &b) {
  if (a.payload.vectorValue == b.payload.vectorValue)
    return true;

  if (a.payload.vectorType != b.payload.vectorType)
    return false;

  if (a.payload.vectorSize != b.payload.vectorSize)
    return false;

  CBVar va{}, vb{};
  va.valueType = vb.valueType = a.payload.vectorType;
  for (uint32_t i = 0; i < a.payload.vectorSize; i++) {
    memcpy(&va.payload, &a.payload.vectorValue[i], sizeof(CBVarPayload));
    memcpy(&vb.payload, &b.payload.vectorValue[i], sizeof(CBVarPayload));
    if (!(va == vb))
      return false;
  }

  return true;
}

inline bool _listEq(const CBVar &a, const CBVar &b) {
  auto next_a = a.payload.listValue.value;
  auto next_b = b.payload.listValue.value;
  while (next_a && next_b) {
    if (*next_a != *next_b)
      return false;

    next_a = a.payload.listValue.next;
    next_b = b.payload.listValue.next;
  }

  // one of the 2 was not null!
  if (next_a || next_b)
    return false;

  // if we reach this point both were null (no nexts)
  // and equal check never failed
  return true;
}

ALWAYS_INLINE inline bool operator==(const CBVar &a, const CBVar &b) {
  if (a.valueType != b.valueType)
    return false;

  switch (a.valueType) {
  case CBType::Any:
  case EndOfBlittableTypes:
    return true;
  case None:
    return a.payload.chainState == b.payload.chainState;
  case Object:
    return a.payload.objectValue == b.payload.objectValue;
  case Enum:
    return a.payload.enumVendorId == b.payload.enumVendorId &&
           a.payload.enumTypeId == b.payload.enumTypeId &&
           a.payload.enumValue == b.payload.enumValue;
  case Bool:
    return a.payload.boolValue == b.payload.boolValue;
  case Int:
    return a.payload.intValue == b.payload.intValue;
  case Float:
    return __builtin_fabs(a.payload.floatValue - b.payload.floatValue) <=
           FLT_EPSILON;
  case Int2: {
    CBInt2 vec = a.payload.int2Value == b.payload.int2Value;
    for (auto i = 0; i < 2; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Int3: {
    CBInt3 vec = a.payload.int3Value == b.payload.int3Value;
    for (auto i = 0; i < 3; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Int4: {
    CBInt4 vec = a.payload.int4Value == b.payload.int4Value;
    for (auto i = 0; i < 4; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Int8: {
    CBInt8 vec = a.payload.int8Value == b.payload.int8Value;
    for (auto i = 0; i < 8; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Int16: {
    auto vec = a.payload.int16Value == b.payload.int16Value;
    for (auto i = 0; i < 16; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Float2: {
    const CBFloat2 vepsi = {FLT_EPSILON, FLT_EPSILON};
    auto diff = a.payload.float2Value - b.payload.float2Value;
    diff[0] = __builtin_fabs(diff[0]);
    diff[1] = __builtin_fabs(diff[1]);
    CBInt2 vec = diff <= vepsi;
    for (auto i = 0; i < 2; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Float3: {
    const CBFloat3 vepsi = {FLT_EPSILON, FLT_EPSILON, FLT_EPSILON};
    auto diff = a.payload.float3Value - b.payload.float3Value;
    diff[0] = __builtin_fabs(diff[0]);
    diff[1] = __builtin_fabs(diff[1]);
    diff[2] = __builtin_fabs(diff[2]);
    CBInt3 vec = diff <= vepsi;
    for (auto i = 0; i < 3; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Float4: {
    const CBFloat4 vepsi = {FLT_EPSILON, FLT_EPSILON, FLT_EPSILON, FLT_EPSILON};
    auto diff = a.payload.float4Value - b.payload.float4Value;
    diff[0] = __builtin_fabs(diff[0]);
    diff[1] = __builtin_fabs(diff[1]);
    diff[2] = __builtin_fabs(diff[2]);
    diff[3] = __builtin_fabs(diff[3]);
    CBInt4 vec = diff <= vepsi;
    for (auto i = 0; i < 4; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Color:
    return a.payload.colorValue.r == b.payload.colorValue.r &&
           a.payload.colorValue.g == b.payload.colorValue.g &&
           a.payload.colorValue.b == b.payload.colorValue.b &&
           a.payload.colorValue.a == b.payload.colorValue.a;
  case Chain:
    return a.payload.chainValue == b.payload.chainValue;
  case Block:
    return a.payload.blockValue == b.payload.blockValue;
  case ContextVar:
  case CBType::String:
    return a.payload.stringValue == b.payload.stringValue ||
           strcmp(a.payload.stringValue, b.payload.stringValue) == 0;
  case Image:
    return a.payload.imageValue.channels == b.payload.imageValue.channels &&
           a.payload.imageValue.width == b.payload.imageValue.width &&
           a.payload.imageValue.height == b.payload.imageValue.height &&
           (a.payload.imageValue.data == b.payload.imageValue.data ||
            memcmp(a.payload.imageValue.data, b.payload.imageValue.data,
                   a.payload.imageValue.channels * a.payload.imageValue.width *
                       a.payload.imageValue.height) == 0);
  case Seq:
    return _seqEq(a, b);
  case Table:
    return _tableEq(a, b);
  case CBType::Bytes:
    return a.payload.bytesSize == b.payload.bytesSize &&
           (a.payload.bytesValue == b.payload.bytesValue ||
            memcmp(a.payload.bytesValue, b.payload.bytesValue,
                   a.payload.bytesSize) == 0);
  case Vector:
    return _vectorEq(a, b);
  case List:
    return _listEq(a, b);
  case Node:
    return a.payload.nodeValue == b.payload.nodeValue;
  case TypeInfo: {
    if (a.payload.typeInfoValue == nullptr) {
      if (b.payload.typeInfoValue == nullptr)
        return true;
      else
        return false;
    }

    if (b.payload.typeInfoValue == nullptr)
      return false;

    return *a.payload.typeInfoValue == *b.payload.typeInfoValue;
  }
  }

  return false;
}

inline bool _seqLess(const CBVar &a, const CBVar &b) {
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

inline bool _tableLess(const CBVar &a, const CBVar &b) {
  auto alen = stbds_shlen(a.payload.tableValue);
  auto blen = stbds_shlen(b.payload.tableValue);
  auto len = std::min(alen, blen);

  for (ptrdiff_t i = 0; i < len; i++) {
    auto sc = strcmp(a.payload.tableValue[i].key, b.payload.tableValue[i].key);
    if (sc < 0)
      return true;
    else if (sc > 0)
      return false;

    auto c = cmp(a.payload.tableValue[i].value, b.payload.tableValue[i].value);
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

inline bool _vectorLess(const CBVar &a, const CBVar &b) {
  auto alen = a.payload.vectorSize;
  auto blen = b.payload.vectorSize;
  auto len = std::min(alen, blen);

  CBVar va{}, vb{};
  va.valueType = vb.valueType = a.payload.vectorType;
  for (uint32_t i = 0; i < len; i++) {
    memcpy(&va.payload, &a.payload.vectorValue[i], sizeof(CBVarPayload));
    memcpy(&vb.payload, &b.payload.vectorValue[i], sizeof(CBVarPayload));
    auto c = cmp(va, vb);
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

inline bool _listLess(const CBVar &a, const CBVar &b) {
  auto next_a = a.payload.listValue.value;
  auto next_b = b.payload.listValue.value;
  while (next_a && next_b) {
    auto c = cmp(*next_a, *next_b);
    if (c < 0)
      return true;
    else if (c > 0)
      return false;

    next_a = a.payload.listValue.next;
    next_b = b.payload.listValue.next;
  }

  // a ended before b!
  if (!next_a && next_b)
    return true;

  return false;
}

ALWAYS_INLINE inline bool operator<(const CBVar &a, const CBVar &b) {
  if (a.valueType != b.valueType)
    return false;

  switch (a.valueType) {
  case None:
    return a.payload.chainState < b.payload.chainState;
  case Enum:
    return a.payload.enumVendorId < b.payload.enumVendorId ||
           a.payload.enumTypeId < b.payload.enumTypeId ||
           a.payload.enumValue < b.payload.enumValue;
  case Bool:
    return a.payload.boolValue < b.payload.boolValue;
  case Int:
    return a.payload.intValue < b.payload.intValue;
  case Float:
    return a.payload.floatValue < b.payload.floatValue;
  case Int2: {
    CBInt2 vec = a.payload.int2Value < b.payload.int2Value;
    for (auto i = 0; i < 2; i++)
      if (vec[i] < 0) // -1 is true
        return true;
    return false;
  }
  case Int3: {
    CBInt3 vec = a.payload.int3Value < b.payload.int3Value;
    for (auto i = 0; i < 3; i++)
      if (vec[i] < 0)
        return true;
    return false;
  }
  case Int4: {
    CBInt4 vec = a.payload.int4Value < b.payload.int4Value;
    for (auto i = 0; i < 4; i++)
      if (vec[i] < 0)
        return true;
    return false;
  }
  case Int8: {
    CBInt8 vec = a.payload.int8Value < b.payload.int8Value;
    for (auto i = 0; i < 8; i++)
      if (vec[i] < 0)
        return true;
    return false;
  }
  case Int16: {
    auto vec = a.payload.int16Value < b.payload.int16Value;
    for (auto i = 0; i < 16; i++)
      if (vec[i] < 0)
        return true;
    return false;
  }
  case Float2: {
    CBInt2 vec = a.payload.float2Value < b.payload.float2Value; // cast to int
    for (auto i = 0; i < 2; i++)
      if (vec[i] < 0)
        return true;
    return false;
  }
  case Float3: {
    CBInt3 vec = a.payload.float3Value < b.payload.float3Value; // cast to int
    for (auto i = 0; i < 3; i++)
      if (vec[i] < 0)
        return true;
    return false;
  }
  case Float4: {
    CBInt4 vec = a.payload.float4Value < b.payload.float4Value; // cast to int
    for (auto i = 0; i < 4; i++)
      if (vec[i] < 0)
        return true;
    return false;
  }
  case Color:
    return a.payload.colorValue.r < b.payload.colorValue.r ||
           a.payload.colorValue.g < b.payload.colorValue.g ||
           a.payload.colorValue.b < b.payload.colorValue.b ||
           a.payload.colorValue.a < b.payload.colorValue.a;
  case ContextVar:
  case CBType::String:
    return strcmp(a.payload.stringValue, b.payload.stringValue) < 0;
  case Image:
    return a.payload.imageValue.channels == b.payload.imageValue.channels &&
           a.payload.imageValue.width == b.payload.imageValue.width &&
           a.payload.imageValue.height == b.payload.imageValue.height &&
           memcmp(a.payload.imageValue.data, b.payload.imageValue.data,
                  a.payload.imageValue.channels * a.payload.imageValue.width *
                      a.payload.imageValue.height) < 0;
  case Seq:
    return _seqLess(a, b);
  case Vector:
    return _vectorLess(a, b);
  case List:
    return _listLess(a, b);
  case Table:
    return _tableLess(a, b);
  case Bytes:
    return a.payload.bytesSize == b.payload.bytesSize &&
           memcmp(a.payload.bytesValue, b.payload.bytesValue,
                  a.payload.bytesSize) < 0;
  case Chain:
  case Block:
  case Object:
  case Node:
  case TypeInfo:
  case CBType::Any:
  case EndOfBlittableTypes:
    return false;
  }

  return false;
}

inline bool _seqLessEq(const CBVar &a, const CBVar &b) {
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

inline bool _tableLessEq(const CBVar &a, const CBVar &b) {
  auto alen = stbds_shlen(a.payload.tableValue);
  auto blen = stbds_shlen(b.payload.tableValue);
  auto len = std::min(alen, blen);

  for (ptrdiff_t i = 0; i < len; i++) {
    auto sc = strcmp(a.payload.tableValue[i].key, b.payload.tableValue[i].key);
    if (sc < 0)
      return true;
    else if (sc > 0)
      return false;

    auto c = cmp(a.payload.tableValue[i].value, b.payload.tableValue[i].value);
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

inline bool _vectorLessEq(const CBVar &a, const CBVar &b) {
  auto alen = a.payload.vectorSize;
  auto blen = b.payload.vectorSize;
  auto len = std::min(alen, blen);

  CBVar va{}, vb{};
  va.valueType = vb.valueType = a.payload.vectorType;
  for (uint32_t i = 0; i < len; i++) {
    memcpy(&va.payload, &a.payload.vectorValue[i], sizeof(CBVarPayload));
    memcpy(&vb.payload, &b.payload.vectorValue[i], sizeof(CBVarPayload));
    auto c = cmp(va, vb);
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

inline bool _listLessEq(const CBVar &a, const CBVar &b) {
  auto next_a = a.payload.listValue.value;
  auto next_b = b.payload.listValue.value;
  while (next_a && next_b) {
    auto c = cmp(*next_a, *next_b);
    if (c < 0)
      return true;
    else if (c > 0)
      return false;

    next_a = a.payload.listValue.next;
    next_b = b.payload.listValue.next;
  }

  // a is bigger case
  if (next_a)
    return false;

  return true;
}

ALWAYS_INLINE inline bool operator<=(const CBVar &a, const CBVar &b) {
  if (a.valueType != b.valueType)
    return false;

  switch (a.valueType) {
  case None:
    return a.payload.chainState <= b.payload.chainState;
  case Enum:
    return a.payload.enumVendorId == b.payload.enumVendorId &&
           a.payload.enumTypeId == b.payload.enumTypeId &&
           a.payload.enumValue <= b.payload.enumValue;
  case Bool:
    return a.payload.boolValue <= b.payload.boolValue;
  case Int:
    return a.payload.intValue <= b.payload.intValue;
  case Float:
    return a.payload.floatValue <= b.payload.floatValue;
  case Int2: {
    CBInt2 vec = a.payload.int2Value <= b.payload.int2Value;
    for (auto i = 0; i < 2; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Int3: {
    CBInt3 vec = a.payload.int3Value <= b.payload.int3Value;
    for (auto i = 0; i < 3; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Int4: {
    CBInt4 vec = a.payload.int4Value <= b.payload.int4Value;
    for (auto i = 0; i < 4; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Int8: {
    CBInt8 vec = a.payload.int8Value <= b.payload.int8Value;
    for (auto i = 0; i < 8; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Int16: {
    auto vec = a.payload.int16Value <= b.payload.int16Value;
    for (auto i = 0; i < 16; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Float2: {
    CBInt2 vec = a.payload.float2Value <= b.payload.float2Value; // cast to int
    for (auto i = 0; i < 2; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Float3: {
    CBInt3 vec = a.payload.float3Value <= b.payload.float3Value; // cast to int
    for (auto i = 0; i < 3; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Float4: {
    CBInt4 vec = a.payload.float4Value <= b.payload.float4Value; // cast to int
    for (auto i = 0; i < 4; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Color:
    return a.payload.colorValue.r <= b.payload.colorValue.r &&
           a.payload.colorValue.g <= b.payload.colorValue.g &&
           a.payload.colorValue.b <= b.payload.colorValue.b &&
           a.payload.colorValue.a <= b.payload.colorValue.a;
  case ContextVar:
  case CBType::String:
    return strcmp(a.payload.stringValue, b.payload.stringValue) <= 0;
  case Image:
    return a.payload.imageValue.channels == b.payload.imageValue.channels &&
           a.payload.imageValue.width == b.payload.imageValue.width &&
           a.payload.imageValue.height == b.payload.imageValue.height &&
           memcmp(a.payload.imageValue.data, b.payload.imageValue.data,
                  a.payload.imageValue.channels * a.payload.imageValue.width *
                      a.payload.imageValue.height) <= 0;
  case Seq:
    return _seqLessEq(a, b);
  case Vector:
    return _vectorLessEq(a, b);
  case List:
    return _listLessEq(a, b);
  case Table:
    return _tableLessEq(a, b);
  case Bytes:
    return a.payload.bytesSize == b.payload.bytesSize &&
           memcmp(a.payload.bytesValue, b.payload.bytesValue,
                  a.payload.bytesSize) <= 0;
  case Chain:
  case Block:
  case Object:
  case Node:
  case TypeInfo:
  case CBType::Any:
  case EndOfBlittableTypes:
    return false;
  }

  return false;
}

ALWAYS_INLINE inline bool operator!=(const CBVar &a, const CBVar &b) {
  return !(a == b);
}

ALWAYS_INLINE inline bool operator>(const CBVar &a, const CBVar &b) {
  return b < a;
}

ALWAYS_INLINE inline bool operator>=(const CBVar &a, const CBVar &b) {
  return b <= a;
}

inline bool operator==(const CBTypeInfo &a, const CBTypeInfo &b) {
  if (a.basicType != b.basicType)
    return false;
  switch (a.basicType) {
  case Object:
    if (a.objectVendorId != b.objectVendorId)
      return false;
    return a.objectTypeId == b.objectTypeId;
  case Enum:
    if (a.enumVendorId != b.enumVendorId)
      return false;
    return a.enumTypeId == b.enumTypeId;
  case Seq: {
    if (a.seqTypes.elements == nullptr && b.seqTypes.elements == nullptr)
      return true;

    if (a.seqTypes.elements && b.seqTypes.elements) {
      if (a.seqTypes.len != b.seqTypes.len)
        return false;
      // compare but allow different orders of elements
      for (uint32_t i = 0; i < a.seqTypes.len; i++) {
        for (uint32_t j = 0; j < b.seqTypes.len; j++) {
          if (a.seqTypes.elements[i] == b.seqTypes.elements[j])
            goto matched;
        }
        return false;
      matched:
        continue;
      }
    } else {
      return false;
    }

    return true;
  }
  case Table: {
    auto atypes = a.tableTypes.len;
    auto btypes = b.tableTypes.len;
    if (atypes != btypes)
      return false;
    for (uint32_t i = 0; i < atypes; i++) {
      if (a.tableTypes.elements[i] != b.tableTypes.elements[i])
        return false;
    }
    return true;
  }
  default:
    return true;
  }
  return true;
}

inline bool operator!=(const CBTypeInfo &a, const CBTypeInfo &b) {
  return !(a == b);
}

inline bool operator!=(const CBExposedTypeInfo &a, const CBExposedTypeInfo &b);

inline bool operator==(const CBExposedTypeInfo &a, const CBExposedTypeInfo &b) {
  if (strcmp(a.name, b.name) != 0 || a.exposedType != b.exposedType ||
      a.isMutable != b.isMutable)
    return false;
  return true;
}

inline bool operator!=(const CBExposedTypeInfo &a, const CBExposedTypeInfo &b) {
  return !(a == b);
}

#endif

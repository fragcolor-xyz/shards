#pragma once

#include "3rdparty/easylogging++.h"
#include "chainblocks.h"

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
  case Any:
    os << "*Any*";
    break;
  case Object:
    os << "Object: " << reinterpret_cast<uintptr_t>(var.payload.objectValue);
    break;
  case Enum:
    os << "Enum: " << var.payload.enumValue;
    break;
  case Bool:
    os << var.payload.boolValue;
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
  case Chain:
    os << "Chain: " << reinterpret_cast<uintptr_t>(var.payload.chainValue);
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
    os << " Channels: " << var.payload.imageValue.channels;
    break;
  case Seq:
    os << "[";
    for (auto i = 0; i < stbds_arrlen(var.payload.seqValue); i++) {
      if (i == 0)
        os << var.payload.seqValue[i];
      else
        os << ", " << var.payload.seqValue[i];
    }
    os << "]";
    break;
  case Table:
    os << "{";
    for (auto i = 0; i < stbds_shlen(var.payload.tableValue); i++) {
      if (i == 0)
        os << var.payload.tableValue[i].key << ": "
           << var.payload.tableValue[i].value;
      else
        os << ", " << var.payload.tableValue[i].key << ": "
           << var.payload.tableValue[i].value;
    }
    os << "}";
    break;
  }
  return os;
}

ALWAYS_INLINE inline bool operator!=(const CBVar &a, const CBVar &b);
ALWAYS_INLINE inline bool operator>(const CBVar &a, const CBVar &b);
ALWAYS_INLINE inline bool operator>=(const CBVar &a, const CBVar &b);

ALWAYS_INLINE inline bool operator==(const CBVar &a, const CBVar &b) {
  if (a.valueType != b.valueType)
    return false;

  switch (a.valueType) {
  case Any:
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
    return a.payload.floatValue == b.payload.floatValue;
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
    CBInt2 vec = a.payload.float2Value == b.payload.float2Value; // cast to int
    for (auto i = 0; i < 2; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Float3: {
    CBInt3 vec = a.payload.float3Value == b.payload.float3Value; // cast to int
    for (auto i = 0; i < 3; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Float4: {
    CBInt4 vec = a.payload.float4Value == b.payload.float4Value; // cast to int
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
    return strcmp(a.payload.stringValue, b.payload.stringValue) == 0;
  case Image:
    return a.payload.imageValue.channels == b.payload.imageValue.channels &&
           a.payload.imageValue.width == b.payload.imageValue.width &&
           a.payload.imageValue.height == b.payload.imageValue.height &&
           memcmp(a.payload.imageValue.data, b.payload.imageValue.data,
                  a.payload.imageValue.channels * a.payload.imageValue.width *
                      a.payload.imageValue.height) == 0;
  case Seq:
    if (stbds_arrlen(a.payload.seqValue) != stbds_arrlen(b.payload.seqValue))
      return false;

    for (auto i = 0; i < stbds_arrlen(a.payload.seqValue); i++) {
      if (a.payload.seqValue[i] != b.payload.seqValue[i])
        return false;
    }

    return true;
  case Table:
    if (stbds_shlen(a.payload.tableValue) != stbds_shlen(b.payload.tableValue))
      return false;

    for (auto i = 0; i < stbds_shlen(a.payload.tableValue); i++) {
      if (strcmp(a.payload.tableValue[i].key, b.payload.tableValue[i].key) != 0)
        return false;

      if (a.payload.tableValue[i].value != b.payload.tableValue[i].value)
        return false;
    }

    return true;
  }

  return false;
}

ALWAYS_INLINE inline bool operator<(const CBVar &a, const CBVar &b) {
  if (a.valueType != b.valueType)
    return false;

  switch (a.valueType) {
  case Any:
  case EndOfBlittableTypes:
    return true;
  case None:
    return a.payload.chainState < b.payload.chainState;
  case Object:
    return a.payload.objectValue < b.payload.objectValue;
  case Enum:
    return a.payload.enumVendorId < b.payload.enumVendorId &&
           a.payload.enumTypeId < b.payload.enumTypeId &&
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
      if (vec[i] < 0)
        return false;
    return true;
  }
  case Int3: {
    CBInt3 vec = a.payload.int3Value < b.payload.int3Value;
    for (auto i = 0; i < 3; i++)
      if (vec[i] < 0)
        return false;
    return true;
  }
  case Int4: {
    CBInt4 vec = a.payload.int4Value < b.payload.int4Value;
    for (auto i = 0; i < 4; i++)
      if (vec[i] < 0)
        return false;
    return true;
  }
  case Int8: {
    CBInt8 vec = a.payload.int8Value < b.payload.int8Value;
    for (auto i = 0; i < 8; i++)
      if (vec[i] < 0)
        return false;
    return true;
  }
  case Int16: {
    auto vec = a.payload.int16Value < b.payload.int16Value;
    for (auto i = 0; i < 16; i++)
      if (vec[i] < 0)
        return false;
    return true;
  }
  case Float2: {
    CBInt2 vec = a.payload.float2Value < b.payload.float2Value; // cast to int
    for (auto i = 0; i < 2; i++)
      if (vec[i] < 0)
        return false;
    return true;
  }
  case Float3: {
    CBInt3 vec = a.payload.float3Value < b.payload.float3Value; // cast to int
    for (auto i = 0; i < 3; i++)
      if (vec[i] < 0)
        return false;
    return true;
  }
  case Float4: {
    CBInt4 vec = a.payload.float4Value < b.payload.float4Value; // cast to int
    for (auto i = 0; i < 4; i++)
      if (vec[i] < 0)
        return false;
    return true;
  }
  case Color:
    return a.payload.colorValue.r < b.payload.colorValue.r &&
           a.payload.colorValue.g < b.payload.colorValue.g &&
           a.payload.colorValue.b < b.payload.colorValue.b &&
           a.payload.colorValue.a < b.payload.colorValue.a;
  case Chain:
    return a.payload.chainValue < b.payload.chainValue;
  case Block:
    return a.payload.blockValue < b.payload.blockValue;
  case ContextVar:
  case CBType::String:
    return strcmp(a.payload.stringValue, b.payload.stringValue) < 0;
  case Image:
    return a.payload.imageValue.channels < b.payload.imageValue.channels &&
           a.payload.imageValue.width < b.payload.imageValue.width &&
           a.payload.imageValue.height < b.payload.imageValue.height &&
           memcmp(a.payload.imageValue.data, b.payload.imageValue.data,
                  a.payload.imageValue.channels * a.payload.imageValue.width *
                      a.payload.imageValue.height) < 0;
  case Seq:
    if (stbds_arrlen(a.payload.seqValue) != stbds_arrlen(b.payload.seqValue))
      return false;

    for (auto i = 0; i < stbds_arrlen(a.payload.seqValue); i++) {
      if (a.payload.seqValue[i] >= b.payload.seqValue[i])
        return false;
    }

    return true;
  case Table:
    if (stbds_shlen(a.payload.tableValue) != stbds_shlen(b.payload.tableValue))
      return false;

    for (auto i = 0; i < stbds_shlen(a.payload.tableValue); i++) {
      if (strcmp(a.payload.tableValue[i].key, b.payload.tableValue[i].key) != 0)
        return false;

      if (a.payload.tableValue[i].value >= b.payload.tableValue[i].value)
        return false;
    }

    return true;
  }

  return false;
}

ALWAYS_INLINE inline bool operator<=(const CBVar &a, const CBVar &b) {
  if (a.valueType != b.valueType)
    return false;

  switch (a.valueType) {
  case Any:
  case EndOfBlittableTypes:
    return true;
  case None:
    return a.payload.chainState <= b.payload.chainState;
  case Object:
    return a.payload.objectValue <= b.payload.objectValue;
  case Enum:
    return a.payload.enumVendorId <= b.payload.enumVendorId &&
           a.payload.enumTypeId <= b.payload.enumTypeId &&
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
      if (vec[i] <= 0)
        return false;
    return true;
  }
  case Int3: {
    CBInt3 vec = a.payload.int3Value <= b.payload.int3Value;
    for (auto i = 0; i < 3; i++)
      if (vec[i] <= 0)
        return false;
    return true;
  }
  case Int4: {
    CBInt4 vec = a.payload.int4Value <= b.payload.int4Value;
    for (auto i = 0; i < 4; i++)
      if (vec[i] <= 0)
        return false;
    return true;
  }
  case Int8: {
    CBInt8 vec = a.payload.int8Value <= b.payload.int8Value;
    for (auto i = 0; i < 8; i++)
      if (vec[i] <= 0)
        return false;
    return true;
  }
  case Int16: {
    auto vec = a.payload.int16Value <= b.payload.int16Value;
    for (auto i = 0; i < 16; i++)
      if (vec[i] <= 0)
        return false;
    return true;
  }
  case Float2: {
    CBInt2 vec = a.payload.float2Value <= b.payload.float2Value; // cast to int
    for (auto i = 0; i < 2; i++)
      if (vec[i] <= 0)
        return false;
    return true;
  }
  case Float3: {
    CBInt3 vec = a.payload.float3Value <= b.payload.float3Value; // cast to int
    for (auto i = 0; i < 3; i++)
      if (vec[i] <= 0)
        return false;
    return true;
  }
  case Float4: {
    CBInt4 vec = a.payload.float4Value <= b.payload.float4Value; // cast to int
    for (auto i = 0; i < 4; i++)
      if (vec[i] <= 0)
        return false;
    return true;
  }
  case Color:
    return a.payload.colorValue.r <= b.payload.colorValue.r &&
           a.payload.colorValue.g <= b.payload.colorValue.g &&
           a.payload.colorValue.b <= b.payload.colorValue.b &&
           a.payload.colorValue.a <= b.payload.colorValue.a;
  case Chain:
    return a.payload.chainValue <= b.payload.chainValue;
  case Block:
    return a.payload.blockValue <= b.payload.blockValue;
  case ContextVar:
  case CBType::String:
    return strcmp(a.payload.stringValue, b.payload.stringValue) <= 0;
  case Image:
    return a.payload.imageValue.channels <= b.payload.imageValue.channels &&
           a.payload.imageValue.width <= b.payload.imageValue.width &&
           a.payload.imageValue.height <= b.payload.imageValue.height &&
           memcmp(a.payload.imageValue.data, b.payload.imageValue.data,
                  a.payload.imageValue.channels * a.payload.imageValue.width *
                      a.payload.imageValue.height) <= 0;
  case Seq:
    if (stbds_arrlen(a.payload.seqValue) != stbds_arrlen(b.payload.seqValue))
      return false;

    for (auto i = 0; i < stbds_arrlen(a.payload.seqValue); i++) {
      if (a.payload.seqValue[i] > b.payload.seqValue[i])
        return false;
    }

    return true;
  case Table:
    if (stbds_shlen(a.payload.tableValue) != stbds_shlen(b.payload.tableValue))
      return false;

    for (auto i = 0; i < stbds_shlen(a.payload.tableValue); i++) {
      if (strcmp(a.payload.tableValue[i].key, b.payload.tableValue[i].key) != 0)
        return false;

      if (a.payload.tableValue[i].value > b.payload.tableValue[i].value)
        return false;
    }

    return true;
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

ALWAYS_INLINE inline bool operator!=(const CBTypeInfo &a, const CBTypeInfo &b);

ALWAYS_INLINE inline bool operator==(const CBTypeInfo &a, const CBTypeInfo &b) {
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
    auto atypes = stbds_arrlen(a.seqTypes);
    auto btypes = stbds_arrlen(b.seqTypes);
    if (atypes != btypes)
      return false;
    for (auto i = 0; i < atypes; i++) {
      if (a.seqTypes[i] != b.seqTypes[i])
        return false;
    }
    return true;
  }
  case Table: {
    auto atypes = stbds_arrlen(a.tableTypes);
    auto btypes = stbds_arrlen(b.tableTypes);
    if (atypes != btypes)
      return false;
    for (auto i = 0; i < atypes; i++) {
      if (a.tableTypes[i] != b.tableTypes[i])
        return false;
    }
    return true;
  }
  default:
    return true;
  }
  return true;
}

ALWAYS_INLINE inline bool operator!=(const CBTypeInfo &a, const CBTypeInfo &b) {
  return !(a == b);
}
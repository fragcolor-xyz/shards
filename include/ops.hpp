/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifndef CB_OPS_HPP
#define CB_OPS_HPP

#include <cassert>
#include <cfloat>
#include <chainblocks.hpp>
#include <string>
#include <string_view>

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
  case CBType::Path:
    name = "Path";
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
  case Array:
    name = "Array";
    break;
  }
  return name;
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

// recursive, likely not inlining
inline bool _seqEq(const CBVar &a, const CBVar &b) {
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

// recursive, likely not inlining
inline bool _tableEq(const CBVar &a, const CBVar &b) {
  auto &ta = a.payload.tableValue;
  auto &tb = a.payload.tableValue;
  if (ta.opaque == tb.opaque)
    return true;

  if (ta.api->tableSize(ta) != ta.api->tableSize(tb))
    return false;

  struct CmpState {
    CBTable tb;
    bool result;
    bool valid;
  } state;
  state.tb = tb;
  state.result = false;
  state.valid = false;
  ta.api->tableForEach(
      ta,
      [](const char *key, CBVar *value, void *data) {
        auto state = (CmpState *)data;
        if (!state->tb.api->tableContains(state->tb, key)) {
          state->result = false;
          state->valid = true;
          return false;
        }

        auto &aval = *value;
        auto &bval = *state->tb.api->tableAt(state->tb, key);
        if (aval != bval) {
          state->result = false;
          state->valid = true;
          return false;
        }
        return true;
      },
      &state);

  if (state.valid)
    return state.result;

  return true;
}

ALWAYS_INLINE inline bool operator==(const CBVar &a, const CBVar &b) {
  if (a.valueType != b.valueType)
    return false;

  switch (a.valueType) {
  case None:
  case CBType::Any:
  case EndOfBlittableTypes:
    return true;
  case Object:
    return a.payload.objectVendorId == b.payload.objectVendorId &&
           a.payload.objectTypeId == b.payload.objectTypeId &&
           a.payload.objectValue == b.payload.objectValue;
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
    CBInt16 vec = a.payload.int16Value == b.payload.int16Value;
    for (auto i = 0; i < 16; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case Float2: {
    const CBFloat2 vepsi = {FLT_EPSILON, FLT_EPSILON};
    CBFloat2 diff = a.payload.float2Value - b.payload.float2Value;
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
    CBFloat3 diff = a.payload.float3Value - b.payload.float3Value;
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
    CBFloat4 diff = a.payload.float4Value - b.payload.float4Value;
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
  case CBType::Path:
  case ContextVar:
  case CBType::String:
    return a.payload.stringValue == b.payload.stringValue ||
           strcmp(a.payload.stringValue, b.payload.stringValue) == 0;
  case Image: {
    auto apixsize = 1;
    auto bpixsize = 1;
    if ((a.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
        CBIMAGE_FLAGS_16BITS_INT)
      apixsize = 2;
    else if ((a.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
             CBIMAGE_FLAGS_32BITS_FLOAT)
      apixsize = 4;
    if ((b.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
        CBIMAGE_FLAGS_16BITS_INT)
      bpixsize = 2;
    else if ((b.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
             CBIMAGE_FLAGS_32BITS_FLOAT)
      bpixsize = 4;
    return apixsize == bpixsize &&
           a.payload.imageValue.channels == b.payload.imageValue.channels &&
           a.payload.imageValue.width == b.payload.imageValue.width &&
           a.payload.imageValue.height == b.payload.imageValue.height &&
           (a.payload.imageValue.data == b.payload.imageValue.data ||
            (memcmp(a.payload.imageValue.data, b.payload.imageValue.data,
                    a.payload.imageValue.channels * a.payload.imageValue.width *
                        a.payload.imageValue.height * apixsize) == 0));
  }
  case Seq:
    return _seqEq(a, b);
  case Table:
    return _tableEq(a, b);
  case CBType::Bytes:
    return a.payload.bytesSize == b.payload.bytesSize &&
           (a.payload.bytesValue == b.payload.bytesValue ||
            memcmp(a.payload.bytesValue, b.payload.bytesValue,
                   a.payload.bytesSize) == 0);
  case CBType::Array:
    return a.payload.arrayValue.len == b.payload.arrayValue.len &&
           a.innerType == b.innerType &&
           (a.payload.arrayValue.elements == b.payload.arrayValue.elements ||
            memcmp(a.payload.arrayValue.elements, b.payload.arrayValue.elements,
                   a.payload.arrayValue.len * sizeof(CBVarPayload)) == 0);
  }

  return false;
}

// recursive, likely not inlining
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

// recursive, likely not inlining
inline bool _tableLess(const CBVar &a, const CBVar &b) {
  auto &ta = a.payload.tableValue;
  auto &tb = a.payload.tableValue;
  if (ta.opaque == tb.opaque)
    return false;

  if (ta.api->tableSize(ta) != ta.api->tableSize(tb))
    return false;

  struct CmpState {
    CBTable ta;
    size_t len;
    bool result;
    bool valid;
  } state;
  state.ta = ta;
  state.len = 0;
  state.result = false;
  state.valid = false;
  tb.api->tableForEach(
      tb,
      [](const char *key, CBVar *value, void *data) {
        auto state = (CmpState *)data;
        state->len++;

        // if a key in tb is not in ta, ta is less
        if (!state->ta.api->tableContains(state->ta, key)) {
          state->result = true;
          state->valid = true;
          return false;
        }

        auto &bval = *value;
        auto &aval = *state->ta.api->tableAt(state->ta, key);
        auto c = cmp(aval, bval);
        if (c < 0) {
          state->result = true;
          state->valid = true;
          return false;
        } else if (c > 0) {
          state->result = false;
          state->valid = true;
          return false;
        }
        return true;
      },
      &state);

  if (state.valid)
    return state.result;

  if (ta.api->tableSize(ta) < state.len)
    return true;
  else
    return false;
}

ALWAYS_INLINE inline bool operator<(const CBVar &a, const CBVar &b) {
  if (a.valueType != b.valueType)
    return false;

  switch (a.valueType) {
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
  case CBType::Path:
  case ContextVar:
  case CBType::String:
    return strcmp(a.payload.stringValue, b.payload.stringValue) < 0;
  case Seq:
    return _seqLess(a, b);
  case Table:
    return _tableLess(a, b);
  case Bytes:
    return a.payload.bytesSize == b.payload.bytesSize &&
           memcmp(a.payload.bytesValue, b.payload.bytesValue,
                  a.payload.bytesSize) < 0;
  case Array:
    return a.payload.arrayValue.len == b.payload.arrayValue.len &&
           a.innerType == b.innerType &&
           memcmp(a.payload.arrayValue.elements, b.payload.arrayValue.elements,
                  a.payload.arrayValue.len * sizeof(CBVarPayload)) < 0;
  case Image:
  case Chain:
  case Block:
  case Object:
  case CBType::Any:
  case None:
  case EndOfBlittableTypes:
    throw chainblocks::InvalidVarTypeError(
        "Comparison operator < not supported for the given type: " +
        type2Name(a.valueType));
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
  auto &ta = a.payload.tableValue;
  auto &tb = a.payload.tableValue;
  if (ta.opaque == tb.opaque)
    return false;

  if (ta.api->tableSize(ta) != ta.api->tableSize(tb))
    return false;

  struct CmpState {
    CBTable ta;
    size_t len;
    bool result;
    bool valid;
  } state;
  state.ta = ta;
  state.len = 0;
  state.result = false;
  state.valid = false;
  tb.api->tableForEach(
      tb,
      [](const char *key, CBVar *value, void *data) {
        auto state = (CmpState *)data;
        state->len++;

        // if a key in tb is not in ta, ta is less
        if (!state->ta.api->tableContains(state->ta, key)) {
          state->result = true;
          state->valid = true;
          return false;
        }

        auto &bval = *value;
        auto &aval = *state->ta.api->tableAt(state->ta, key);
        auto c = cmp(aval, bval);
        if (c < 0) {
          state->result = true;
          state->valid = true;
          return false;
        } else if (c > 0) {
          state->result = false;
          state->valid = true;
          return false;
        }
        return true;
      },
      &state);

  if (state.valid)
    return state.result;

  if (ta.api->tableSize(ta) <= state.len)
    return true;
  else
    return false;
}

ALWAYS_INLINE inline bool operator<=(const CBVar &a, const CBVar &b) {
  if (a.valueType != b.valueType)
    return false;

  switch (a.valueType) {
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
  case CBType::Path:
  case ContextVar:
  case CBType::String:
    return strcmp(a.payload.stringValue, b.payload.stringValue) <= 0;
  case Seq:
    return _seqLessEq(a, b);
  case Table:
    return _tableLessEq(a, b);
  case Bytes:
    return a.payload.bytesSize == b.payload.bytesSize &&
           memcmp(a.payload.bytesValue, b.payload.bytesValue,
                  a.payload.bytesSize) <= 0;
  case Array:
    return a.payload.arrayValue.len == b.payload.arrayValue.len &&
           a.innerType == b.innerType &&
           memcmp(a.payload.arrayValue.elements, b.payload.arrayValue.elements,
                  a.payload.arrayValue.len * sizeof(CBVarPayload)) <= 0;
  case Image:
  case Chain:
  case Block:
  case Object:
  case CBType::Any:
  case None:
  case EndOfBlittableTypes:
    throw chainblocks::InvalidVarTypeError(
        "Comparison operator <= not supported for the given type: " +
        type2Name(a.valueType));
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

namespace std {
// TODO put in its own header maybe

template <> struct hash<CBTypeInfo> {
  std::size_t operator()(const CBTypeInfo &typeInfo) const {
    using std::hash;
    using std::size_t;
    using std::string;
    auto res = hash<int>()(typeInfo.basicType);
    if (typeInfo.basicType == Table) {
      if (typeInfo.table.keys.elements) {
        for (uint32_t i = 0; i < typeInfo.table.keys.len; i++) {
          res = res ^ hash<string>()(typeInfo.table.keys.elements[i]);
        }
      }
      if (typeInfo.table.types.elements) {
        for (uint32_t i = 0; i < typeInfo.table.types.len; i++) {
          res = res ^ hash<CBTypeInfo>()(typeInfo.table.types.elements[i]);
        }
      }
    } else if (typeInfo.basicType == Seq) {
      for (uint32_t i = 0; i < typeInfo.seqTypes.len; i++) {
        res = res ^ hash<CBTypeInfo>()(typeInfo.seqTypes.elements[i]);
      }
    } else if (typeInfo.basicType == Object) {
      res = res ^ hash<int>()(typeInfo.object.vendorId);
      res = res ^ hash<int>()(typeInfo.object.typeId);
    } else if (typeInfo.basicType == Enum) {
      res = res ^ hash<int>()(typeInfo.enumeration.vendorId);
      res = res ^ hash<int>()(typeInfo.enumeration.typeId);
    }
    return res;
  }
};

template <> struct hash<CBExposedTypeInfo> {
  std::size_t operator()(const CBExposedTypeInfo &typeInfo) const {
    using std::hash;
    using std::size_t;
    using std::string;
    auto res = hash<string>()(typeInfo.name);
    res = res ^ hash<CBTypeInfo>()(typeInfo.exposedType);
    res = res ^ hash<int>()(typeInfo.isMutable);
    res = res ^ hash<int>()(typeInfo.isTableEntry);
    return res;
  }
};

#define MAGIC_HASH(__val__) res = res ^ hash<decltype(__val__)>()(__val__)
#define MAGIC_HASH2(__val__)                                                   \
  res = res ^                                                                  \
        hash<std::remove_const<                                                \
            std::remove_reference<decltype(__val__)>::type>::type>()(__val__)
template <> struct hash<CBVar> {
  std::size_t operator()(const CBVar &var) const {
    using std::hash;
    using std::size_t;
    using std::string;

    static CBCore core{};
    if (!core.registerBlock) {
      if (!chainblocksInterface(CHAINBLOCKS_CURRENT_ABI, &core))
        throw std::runtime_error("chainblocksInterface failed!");
    }

    auto res = hash<int>()(int(var.valueType));
    switch (var.valueType) {
    case None:
    case Any:
      break;
    case Object:
      MAGIC_HASH(var.payload.objectVendorId);
      MAGIC_HASH(var.payload.objectTypeId);
      MAGIC_HASH(var.payload.objectValue);
      break;
    case Enum:
      MAGIC_HASH(var.payload.enumVendorId);
      MAGIC_HASH(var.payload.enumTypeId);
      MAGIC_HASH(var.payload.enumValue);
      break;
    case Bool:
      MAGIC_HASH(var.payload.boolValue);
      break;
    case Int:
      MAGIC_HASH(var.payload.intValue);
      break;
    case Int2:
      for (auto i = 0; i < 2; i++) {
        MAGIC_HASH2(var.payload.int2Value[i]);
      }
      break;
    case Int3:
      for (auto i = 0; i < 3; i++) {
        MAGIC_HASH2(var.payload.int3Value[i]);
      }
      break;
    case Int4:
      for (auto i = 0; i < 4; i++) {
        MAGIC_HASH2(var.payload.int4Value[i]);
      }
      break;
    case Int8:
      for (auto i = 0; i < 8; i++) {
        MAGIC_HASH2(var.payload.int8Value[i]);
      }
      break;
    case Int16:
      for (auto i = 0; i < 16; i++) {
        MAGIC_HASH2(var.payload.int16Value[i]);
      }
      break;
    case Float:
      MAGIC_HASH(var.payload.floatValue);
      break;
    case Float2:
      for (auto i = 0; i < 2; i++) {
        MAGIC_HASH2(var.payload.float2Value[i]);
      }
      break;
    case Float3:
      for (auto i = 0; i < 3; i++) {
        MAGIC_HASH2(var.payload.float3Value[i]);
      }
      break;
    case Float4:
      for (auto i = 0; i < 4; i++) {
        MAGIC_HASH2(var.payload.float4Value[i]);
      }
      break;
    case Color:
      MAGIC_HASH(var.payload.colorValue.r);
      MAGIC_HASH(var.payload.colorValue.g);
      MAGIC_HASH(var.payload.colorValue.b);
      MAGIC_HASH(var.payload.colorValue.a);
      break;
    case Chain: {
      auto chainInfo = core.getChainInfo(var.payload.chainValue);
      std::string_view buf(chainInfo.name);
      res = res ^ hash<std::string_view>()(buf);
      MAGIC_HASH(chainInfo.looped);
      MAGIC_HASH(chainInfo.unsafe);
      for (uint32_t i = 0; i < chainInfo.blocks.len; i++) {
        CBVar blockVar{};
        blockVar.valueType = CBType::Block;
        blockVar.payload.blockValue = chainInfo.blocks.elements[i];
        res = res ^ hash<CBVar>()(blockVar);
      }
    } break;
    case Block: {
      auto blk = var.payload.blockValue;
      std::string_view buf(blk->name(blk));
      res = res ^ hash<std::string_view>()(buf);
      auto params = blk->parameters(blk);
      for (uint32_t i = 0; i < params.len; i++) {
        res = res ^ hash<CBVar>()(blk->getParam(blk, int(i)));
      }
    } break;
    case Bytes: {
      std::string_view buf((char *)var.payload.bytesValue,
                           var.payload.bytesSize);
      res = res ^ hash<std::string_view>()(buf);
    } break;
    case Array: {
      res = res ^ hash<int>()(int(var.innerType));
      std::string_view buf((char *)var.payload.arrayValue.elements,
                           var.payload.arrayValue.len * sizeof(CBVarPayload));
      res = res ^ hash<std::string_view>()(buf);
    } break;
    case String:
    case Path:
    case ContextVar: {
      std::string_view buf(var.payload.stringValue);
      res = res ^ hash<std::string_view>()(buf);
    } break;
    case Image: {
      auto pixsize = 1;
      if ((var.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
          CBIMAGE_FLAGS_16BITS_INT)
        pixsize = 2;
      else if ((var.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
               CBIMAGE_FLAGS_32BITS_FLOAT)
        pixsize = 4;
      MAGIC_HASH(var.payload.imageValue.width);
      MAGIC_HASH(var.payload.imageValue.height);
      MAGIC_HASH(var.payload.imageValue.channels);
      MAGIC_HASH(var.payload.imageValue.flags);
      auto len = size_t(var.payload.imageValue.width) *
                 size_t(var.payload.imageValue.height) *
                 size_t(var.payload.imageValue.channels) * pixsize;
      std::string_view buf((char *)var.payload.imageValue.data, len);
      res = res ^ hash<std::string_view>()(buf);
    } break;
    case Seq:
      for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
        res = res ^ hash<CBVar>()(var.payload.seqValue.elements[i]);
      }
      break;
    case Table: {
      auto table = var.payload.tableValue;
      table.api->tableForEach(
          table,
          [](const char *key, CBVar *value, void *data) {
            std::size_t *pres = (std::size_t *)data;
            std::string_view buf(key);
            *pres = *pres ^ hash<std::string_view>()(buf);
            *pres = *pres ^ hash<CBVar>()(*value);
            return true;
          },
          &res);
    } break;
    case EndOfBlittableTypes:
      break;
    }
    return res;
  }
};
} // namespace std

#endif

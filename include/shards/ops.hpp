/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_OPS_HPP
#define SH_OPS_HPP

#include <cassert>
#include <cfloat>
#include "shards.hpp"
#include <string>
#include <string_view>

inline const char *type2Name_raw(SHType type) {
  switch (type) {
  case SHType::EndOfBlittableTypes:
    shassert("Invalid type");
    return "";
  case SHType::Type:
    return "Type";
  case SHType::None:
    return "None";
  case SHType::Any:
    return "Any";
  case SHType::Object:
    return "Object";
  case SHType::Enum:
    return "Enum";
  case SHType::Bool:
    return "Bool";
  case SHType::Bytes:
    return "Bytes";
  case SHType::Int:
    return "Int";
  case SHType::Int2:
    return "Int2";
  case SHType::Int3:
    return "Int3";
  case SHType::Int4:
    return "Int4";
  case SHType::Int8:
    return "Int8";
  case SHType::Int16:
    return "Int16";
  case SHType::Float:
    return "Float";
  case SHType::Float2:
    return "Float2";
  case SHType::Float3:
    return "Float3";
  case SHType::Float4:
    return "Float4";
  case SHType::Color:
    return "Color";
  case SHType::Wire:
    return "Wire";
  case SHType::ShardRef:
    return "Shard";
  case SHType::String:
    return "String";
  case SHType::ContextVar:
    return "ContextVar";
  case SHType::Path:
    return "Path";
  case SHType::Image:
    return "Image";
  case SHType::Audio:
    return "Audio";
  case SHType::Seq:
    return "Seq";
  case SHType::Table:
    return "Table";
  case SHType::Set:
    return "Set";
  case SHType::Array:
    return "Array";
  case SHType::Trait:
    return "Trait";
  }
  return "";
}

inline std::string type2Name(SHType type) { return type2Name_raw(type); }

ALWAYS_INLINE inline bool operator!=(const SHVar &a, const SHVar &b);
ALWAYS_INLINE inline bool operator<(const SHVar &a, const SHVar &b);
ALWAYS_INLINE inline bool operator>(const SHVar &a, const SHVar &b);
ALWAYS_INLINE inline bool operator>=(const SHVar &a, const SHVar &b);
ALWAYS_INLINE inline bool operator==(const SHVar &a, const SHVar &b);

bool operator==(const SHTypeInfo &a, const SHTypeInfo &b);
inline bool operator!=(const SHTypeInfo &a, const SHTypeInfo &b);

inline int cmp(const SHVar &a, const SHVar &b) {
  if (a == b)
    return 0;
  else if (a < b)
    return -1;
  else
    return 1;
}

bool _seqEq(const SHVar &a, const SHVar &b);

bool _setEq(const SHVar &a, const SHVar &b);

bool _tableEq(const SHVar &a, const SHVar &b);

ALWAYS_INLINE inline bool operator==(const SHVar &a, const SHVar &b) {
  if (a.valueType != b.valueType)
    return false;

  switch (a.valueType) {
  case SHType::Type:
    return *a.payload.typeValue == *b.payload.typeValue;
  case SHType::None:
  case SHType::Any:
  case SHType::EndOfBlittableTypes:
    return true;
  case SHType::Object:
    return a.payload.objectVendorId == b.payload.objectVendorId && a.payload.objectTypeId == b.payload.objectTypeId &&
           a.payload.objectValue == b.payload.objectValue;
  case SHType::Enum:
    return a.payload.enumVendorId == b.payload.enumVendorId && a.payload.enumTypeId == b.payload.enumTypeId &&
           a.payload.enumValue == b.payload.enumValue;
  case SHType::Bool:
    return a.payload.boolValue == b.payload.boolValue;
  case SHType::Int:
    return a.payload.intValue == b.payload.intValue;
  case SHType::Float:
    return __builtin_fabs(a.payload.floatValue - b.payload.floatValue) <= DBL_EPSILON;
  case SHType::Int2: {
    SHInt2 vec = a.payload.int2Value == b.payload.int2Value;
    for (auto i = 0; i < 2; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case SHType::Int3: {
    SHInt3 vec = a.payload.int3Value == b.payload.int3Value;
    for (auto i = 0; i < 3; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case SHType::Int4: {
    SHInt4 vec = a.payload.int4Value == b.payload.int4Value;
    for (auto i = 0; i < 4; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case SHType::Int8: {
    SHInt8 vec = a.payload.int8Value == b.payload.int8Value;
    for (auto i = 0; i < 8; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case SHType::Int16: {
    SHInt16 vec = a.payload.int16Value == b.payload.int16Value;
    for (auto i = 0; i < 16; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case SHType::Float2: {
    const SHFloat2 vepsi = {DBL_EPSILON, DBL_EPSILON};
    SHFloat2 diff = a.payload.float2Value - b.payload.float2Value;
    diff[0] = __builtin_fabs(diff[0]);
    diff[1] = __builtin_fabs(diff[1]);
    SHInt2 vec = diff <= vepsi;
    for (auto i = 0; i < 2; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case SHType::Float3: {
    const SHFloat3 vepsi = {FLT_EPSILON, FLT_EPSILON, FLT_EPSILON};
    SHFloat3 diff = a.payload.float3Value - b.payload.float3Value;
    diff[0] = __builtin_fabs(diff[0]);
    diff[1] = __builtin_fabs(diff[1]);
    diff[2] = __builtin_fabs(diff[2]);
    SHInt3 vec = diff <= vepsi;
    for (auto i = 0; i < 3; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case SHType::Float4: {
    const SHFloat4 vepsi = {FLT_EPSILON, FLT_EPSILON, FLT_EPSILON, FLT_EPSILON};
    SHFloat4 diff = a.payload.float4Value - b.payload.float4Value;
    diff[0] = __builtin_fabs(diff[0]);
    diff[1] = __builtin_fabs(diff[1]);
    diff[2] = __builtin_fabs(diff[2]);
    diff[3] = __builtin_fabs(diff[3]);
    SHInt4 vec = diff <= vepsi;
    for (auto i = 0; i < 4; i++)
      if (vec[i] == 0)
        return false;
    return true;
  }
  case SHType::Color:
    return a.payload.colorValue.r == b.payload.colorValue.r && a.payload.colorValue.g == b.payload.colorValue.g &&
           a.payload.colorValue.b == b.payload.colorValue.b && a.payload.colorValue.a == b.payload.colorValue.a;
  case SHType::Wire: {
    auto aWire = reinterpret_cast<const std::shared_ptr<SHWire> *>(a.payload.wireValue);
    auto bWire = reinterpret_cast<const std::shared_ptr<SHWire> *>(b.payload.wireValue);
    return *aWire == *bWire;
  }
  case SHType::ShardRef:
    return a.payload.shardValue == b.payload.shardValue;
  case SHType::Path:
  case SHType::ContextVar:
  case SHType::String: {
    if (a.payload.stringValue == b.payload.stringValue && a.payload.stringLen == b.payload.stringLen)
      return true;

    const auto astr = a.payload.stringLen > 0 ? std::string_view(a.payload.stringValue, a.payload.stringLen)
                                              : std::string_view(a.payload.stringValue);

    const auto bstr = b.payload.stringLen > 0 ? std::string_view(b.payload.stringValue, b.payload.stringLen)
                                              : std::string_view(b.payload.stringValue);

    return astr == bstr;
  }
  case SHType::Image: {
    auto apixsize = 1;
    auto bpixsize = 1;
    if ((a.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      apixsize = 2;
    else if ((a.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      apixsize = 4;
    if ((b.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      bpixsize = 2;
    else if ((b.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      bpixsize = 4;
    return apixsize == bpixsize && a.payload.imageValue.channels == b.payload.imageValue.channels &&
           a.payload.imageValue.width == b.payload.imageValue.width &&
           a.payload.imageValue.height == b.payload.imageValue.height &&
           (a.payload.imageValue.data == b.payload.imageValue.data ||
            (memcmp(a.payload.imageValue.data, b.payload.imageValue.data,
                    a.payload.imageValue.channels * a.payload.imageValue.width * a.payload.imageValue.height * apixsize) == 0));
  }
  case SHType::Audio: {
    return a.payload.audioValue.nsamples == b.payload.audioValue.nsamples &&
           a.payload.audioValue.channels == b.payload.audioValue.channels &&
           a.payload.audioValue.sampleRate == b.payload.audioValue.sampleRate &&
           (a.payload.audioValue.samples == b.payload.audioValue.samples ||
            (memcmp(a.payload.audioValue.samples, b.payload.audioValue.samples,
                    a.payload.audioValue.channels * a.payload.audioValue.nsamples * sizeof(float)) == 0));
  }
  case SHType::Seq:
    return _seqEq(a, b);
  case SHType::Table:
    return _tableEq(a, b);
  case SHType::Set:
    return _setEq(a, b);
  case SHType::Bytes:
    return a.payload.bytesSize == b.payload.bytesSize &&
           (a.payload.bytesValue == b.payload.bytesValue ||
            memcmp(a.payload.bytesValue, b.payload.bytesValue, a.payload.bytesSize) == 0);
  case SHType::Array:
    return a.payload.arrayValue.len == b.payload.arrayValue.len && a.innerType == b.innerType &&
           (a.payload.arrayValue.elements == b.payload.arrayValue.elements ||
            memcmp(a.payload.arrayValue.elements, b.payload.arrayValue.elements,
                   a.payload.arrayValue.len * sizeof(SHVarPayload)) == 0);
  case SHType::Trait:
    return memcmp(a.payload.traitValue->id, b.payload.traitValue->id, sizeof(SHTrait::id)) == 0;
  }

  return false;
}

bool _seqLess(const SHVar &a, const SHVar &b);

bool _tableLess(const SHVar &a, const SHVar &b);

// avoid trying to be smart with SIMDs here
// compiler will outsmart us likely anyway.
#define SHVECTOR_CMP(_a_, _b_, _s_, _final_) \
  for (auto i = 0; i < _s_; i++) {           \
    if (_a_[i] < _b_[i])                     \
      return true;                           \
    else if (_a_[i] > _b_[i])                \
      return false;                          \
  }                                          \
  return _final_

ALWAYS_INLINE inline bool operator<(const SHVar &a, const SHVar &b) {
  if (a.valueType != b.valueType)
    return a.valueType < b.valueType;

  switch (a.valueType) {
  case SHType::None:
    return false;
  case SHType::Any:
    return false;
  case SHType::Enum:
    if (a.payload.enumVendorId != b.payload.enumVendorId)
      return a.payload.enumVendorId < b.payload.enumVendorId;
    if (a.payload.enumTypeId != b.payload.enumTypeId)
      return a.payload.enumTypeId < b.payload.enumTypeId;
    return a.payload.enumValue < b.payload.enumValue;
  case SHType::Bool:
    return a.payload.boolValue < b.payload.boolValue;
  case SHType::Int:
    return a.payload.intValue < b.payload.intValue;
  case SHType::Float:
    return a.payload.floatValue < b.payload.floatValue;
  case SHType::Int2: {
    SHVECTOR_CMP(a.payload.int2Value, b.payload.int2Value, 2, false);
  }
  case SHType::Int3: {
    SHVECTOR_CMP(a.payload.int3Value, b.payload.int3Value, 3, false);
  }
  case SHType::Int4: {
    SHVECTOR_CMP(a.payload.int4Value, b.payload.int4Value, 4, false);
  }
  case SHType::Int8: {
    SHVECTOR_CMP(a.payload.int8Value, b.payload.int8Value, 8, false);
  }
  case SHType::Int16: {
    SHVECTOR_CMP(a.payload.int16Value, b.payload.int16Value, 16, false);
  }
  case SHType::Float2: {
    SHVECTOR_CMP(a.payload.float2Value, b.payload.float2Value, 2, false);
  }
  case SHType::Float3: {
    SHVECTOR_CMP(a.payload.float3Value, b.payload.float3Value, 3, false);
  }
  case SHType::Float4: {
    SHVECTOR_CMP(a.payload.float4Value, b.payload.float4Value, 4, false);
  }
  case SHType::Color:
    return a.payload.colorValue.r < b.payload.colorValue.r || a.payload.colorValue.g < b.payload.colorValue.g ||
           a.payload.colorValue.b < b.payload.colorValue.b || a.payload.colorValue.a < b.payload.colorValue.a;
  case SHType::Bytes: {
    if (a.payload.bytesValue == b.payload.bytesValue && a.payload.bytesSize == b.payload.bytesSize)
      return false;
    std::string_view abuf((const char *)a.payload.bytesValue, a.payload.bytesSize);
    std::string_view bbuf((const char *)b.payload.bytesValue, b.payload.bytesSize);
    return abuf < bbuf;
  }
  case SHType::Path:
  case SHType::ContextVar:
  case SHType::String: {
    if (a.payload.stringValue == b.payload.stringValue && a.payload.stringLen == b.payload.stringLen)
      return false;

    const auto astr = a.payload.stringLen > 0 ? std::string_view(a.payload.stringValue, a.payload.stringLen)
                                              : std::string_view(a.payload.stringValue);

    const auto bstr = b.payload.stringLen > 0 ? std::string_view(b.payload.stringValue, b.payload.stringLen)
                                              : std::string_view(b.payload.stringValue);

    return astr < bstr;
  }
  case SHType::Image:
    return a.payload.imageValue.data < b.payload.imageValue.data;
  case SHType::Seq:
    return _seqLess(a, b);
  case SHType::Table:
    return _tableLess(a, b);
  case SHType::Wire:
    return a.payload.wireValue < b.payload.wireValue;
  case SHType::ShardRef:
    return a.payload.shardValue < b.payload.shardValue;
  case SHType::Object:
    if (a.payload.objectVendorId != b.payload.objectVendorId)
      return a.payload.objectVendorId < b.payload.objectVendorId;
    if (a.payload.objectTypeId != b.payload.objectTypeId)
      return a.payload.objectTypeId < b.payload.objectTypeId;
    return a.payload.objectValue < b.payload.objectValue;
  case SHType::Array: {
    if (a.payload.arrayValue.elements == b.payload.arrayValue.elements && a.payload.arrayValue.len == b.payload.arrayValue.len)
      return false;
    std::string_view abuf((const char *)a.payload.arrayValue.elements, a.payload.arrayValue.len * sizeof(SHVarPayload));
    std::string_view bbuf((const char *)b.payload.arrayValue.elements, b.payload.arrayValue.len * sizeof(SHVarPayload));
    return abuf < bbuf;
  }
  case SHType::Audio:
    return a.payload.audioValue.samples < b.payload.audioValue.samples;
  case SHType::Type:
    return a.payload.typeValue < b.payload.typeValue;
  case SHType::Trait: {
    auto &id0 = a.payload.traitValue->id;
    auto &id1 = b.payload.traitValue->id;
    if (id0[0] == id1[0])
      return id0[1] < id1[1];
    return id0[0] < id1[0];
  }
  case SHType::Set:
  case SHType::EndOfBlittableTypes:
    shassert("Invalid type");
    return false;
  }
}

bool _seqLessEq(const SHVar &a, const SHVar &b);

bool _tableLessEq(const SHVar &a, const SHVar &b);

ALWAYS_INLINE inline bool operator<=(const SHVar &a, const SHVar &b) {
  if (a.valueType != b.valueType)
    return a.valueType < b.valueType;

  switch (a.valueType) {
  case SHType::Enum: {
    if (a.payload.enumVendorId != b.payload.enumVendorId)
      return a.payload.enumVendorId < b.payload.enumVendorId;
    if (a.payload.enumTypeId != b.payload.enumTypeId)
      return a.payload.enumTypeId < b.payload.enumTypeId;
    return a.payload.enumValue <= b.payload.enumValue;
  }
  case SHType::Bool:
    return a.payload.boolValue <= b.payload.boolValue;
  case SHType::Int:
    return a.payload.intValue <= b.payload.intValue;
  case SHType::Float:
    return a.payload.floatValue <= b.payload.floatValue;
  case SHType::Int2: {
    SHVECTOR_CMP(a.payload.int2Value, b.payload.int2Value, 2, true);
  }
  case SHType::Int3: {
    SHVECTOR_CMP(a.payload.int3Value, b.payload.int3Value, 3, true);
  }
  case SHType::Int4: {
    SHVECTOR_CMP(a.payload.int4Value, b.payload.int4Value, 4, true);
  }
  case SHType::Int8: {
    SHVECTOR_CMP(a.payload.int8Value, b.payload.int8Value, 8, true);
  }
  case SHType::Int16: {
    SHVECTOR_CMP(a.payload.int16Value, b.payload.int16Value, 16, true);
  }
  case SHType::Float2: {
    SHVECTOR_CMP(a.payload.float2Value, b.payload.float2Value, 2, true);
  }
  case SHType::Float3: {
    SHVECTOR_CMP(a.payload.float3Value, b.payload.float3Value, 3, true);
  }
  case SHType::Float4: {
    SHVECTOR_CMP(a.payload.float4Value, b.payload.float4Value, 4, true);
  }
  case SHType::Color:
    return a.payload.colorValue.r <= b.payload.colorValue.r && a.payload.colorValue.g <= b.payload.colorValue.g &&
           a.payload.colorValue.b <= b.payload.colorValue.b && a.payload.colorValue.a <= b.payload.colorValue.a;
  case SHType::Bytes: {
    if (a.payload.bytesValue == b.payload.bytesValue && a.payload.bytesSize == b.payload.bytesSize)
      return true;
    std::string_view abuf((const char *)a.payload.bytesValue, a.payload.bytesSize);
    std::string_view bbuf((const char *)b.payload.bytesValue, b.payload.bytesSize);
    return abuf <= bbuf;
  }
  case SHType::Path:
  case SHType::ContextVar:
  case SHType::String: {
    if (a.payload.stringValue == b.payload.stringValue && a.payload.stringLen == b.payload.stringLen)
      return true;

    const auto astr = a.payload.stringLen > 0 ? std::string_view(a.payload.stringValue, a.payload.stringLen)
                                              : std::string_view(a.payload.stringValue);

    const auto bstr = b.payload.stringLen > 0 ? std::string_view(b.payload.stringValue, b.payload.stringLen)
                                              : std::string_view(b.payload.stringValue);

    return astr <= bstr;
  }
  case SHType::Image:
    return a.payload.imageValue.data <= b.payload.imageValue.data;
  case SHType::Seq:
    return _seqLessEq(a, b);
  case SHType::Table:
    return _tableLessEq(a, b);
  case SHType::Wire:
    return a.payload.wireValue <= b.payload.wireValue;
  case SHType::ShardRef:
    return a.payload.shardValue <= b.payload.shardValue;
  case SHType::Object:
    if (a.payload.objectVendorId != b.payload.objectVendorId)
      return a.payload.objectVendorId < b.payload.objectVendorId;
    if (a.payload.objectTypeId != b.payload.objectTypeId)
      return a.payload.objectTypeId < b.payload.objectTypeId;
    return a.payload.objectValue <= b.payload.objectValue;
  case SHType::Array: {
    if (a.payload.arrayValue.elements == b.payload.arrayValue.elements && a.payload.arrayValue.len == b.payload.arrayValue.len)
      return true;
    std::string_view abuf((const char *)a.payload.arrayValue.elements, a.payload.arrayValue.len * sizeof(SHVarPayload));
    std::string_view bbuf((const char *)b.payload.arrayValue.elements, b.payload.arrayValue.len * sizeof(SHVarPayload));
    return abuf <= bbuf;
  }
  case SHType::Audio:
    return a.payload.audioValue.samples <= b.payload.audioValue.samples;
  case SHType::Type:
    return a.payload.typeValue <= b.payload.typeValue;
  case SHType::Trait: {
    auto &id0 = a.payload.traitValue->id;
    auto &id1 = b.payload.traitValue->id;
    if (id0[0] == id1[0])
      return id0[1] <= id1[1];
    return id0[0] <= id1[0];
  }
  case SHType::Set:
  case SHType::EndOfBlittableTypes:
    shassert("Invalid type");
    return false;
  case SHType::None:
    return true;
  case SHType::Any:
    return true;
  }
}

#undef SHVECTOR_CMP

ALWAYS_INLINE inline bool operator!=(const SHVar &a, const SHVar &b) { return !(a == b); }

ALWAYS_INLINE inline bool operator>(const SHVar &a, const SHVar &b) { return b < a; }

ALWAYS_INLINE inline bool operator>=(const SHVar &a, const SHVar &b) { return b <= a; }

inline bool operator!=(const SHTypeInfo &a, const SHTypeInfo &b) { return !(a == b); }

inline bool operator!=(const SHExposedTypeInfo &a, const SHExposedTypeInfo &b);

inline bool operator==(const SHExposedTypeInfo &a, const SHExposedTypeInfo &b) {
  if (strcmp(a.name, b.name) != 0 || a.exposedType != b.exposedType || a.isMutable != b.isMutable ||
      a.isProtected != b.isProtected || a.global != b.global)
    return false;
  return true;
}

inline bool operator!=(const SHExposedTypeInfo &a, const SHExposedTypeInfo &b) { return !(a == b); }

bool _almostEqual(const SHVar &lhs, const SHVar &rhs, double e);

namespace shards {
SHVar hash(const SHVar &var);
} // namespace shards

namespace std {
template <> struct hash<SHVar> {
  std::size_t operator()(const SHVar &var) const {
    // not ideal on 32 bits as our hash is 64.. but it should be ok
    return std::size_t(shards::hash(var).payload.int2Value[0]);
  }
};

template <> struct hash<SHTypeInfo> {
  std::size_t operator()(const SHTypeInfo &typeInfo) const {
    using std::hash;
    using std::size_t;
    using std::string;
    auto res = hash<int>()(int(typeInfo.basicType));
    res = res ^ hash<int>()(int(typeInfo.innerType));
    if (typeInfo.basicType == SHType::Table) {
      if (typeInfo.table.keys.elements) {
        for (uint32_t i = 0; i < typeInfo.table.keys.len; i++) {
          res = res ^ hash<SHVar>()(typeInfo.table.keys.elements[i]);
        }
      }
      if (typeInfo.table.types.elements) {
        for (uint32_t i = 0; i < typeInfo.table.types.len; i++) {
          res = res ^ hash<SHTypeInfo>()(typeInfo.table.types.elements[i]);
        }
      }
    } else if (typeInfo.basicType == SHType::Seq) {
      for (uint32_t i = 0; i < typeInfo.seqTypes.len; i++) {
        if (typeInfo.seqTypes.elements[i].recursiveSelf) {
          res = res ^ hash<int>()(INT32_MAX);
        } else {
          res = res ^ hash<SHTypeInfo>()(typeInfo.seqTypes.elements[i]);
        }
      }
    } else if (typeInfo.basicType == SHType::Set) {
      for (uint32_t i = 0; i < typeInfo.setTypes.len; i++) {
        if (typeInfo.setTypes.elements[i].recursiveSelf) {
          res = res ^ hash<int>()(INT32_MAX);
        } else {
          res = res ^ hash<SHTypeInfo>()(typeInfo.setTypes.elements[i]);
        }
      }
    } else if (typeInfo.basicType == SHType::Object) {
      res = res ^ hash<int>()(typeInfo.object.vendorId);
      res = res ^ hash<int>()(typeInfo.object.typeId);
    } else if (typeInfo.basicType == SHType::Enum) {
      res = res ^ hash<int>()(typeInfo.enumeration.vendorId);
      res = res ^ hash<int>()(typeInfo.enumeration.typeId);
    }
    return res;
  }
};

template <> struct hash<SHExposedTypeInfo> {
  std::size_t operator()(const SHExposedTypeInfo &typeInfo) const {
    using std::hash;
    using std::size_t;
    using std::string;
    auto res = hash<string>()(typeInfo.name);
    res = res ^ hash<SHTypeInfo>()(typeInfo.exposedType);
    res = res ^ hash<int>()(typeInfo.isMutable);
    res = res ^ hash<int>()(typeInfo.isProtected);
    res = res ^ hash<int>()(typeInfo.global);
    return res;
  }
};
} // namespace std

#endif

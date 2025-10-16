/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_OPS_HPP
#define SH_OPS_HPP

#include <cassert>
#include <cfloat>
#include "shards.hpp"
#include <string>
#include <string_view>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#ifdef __SSE2__
#include <emmintrin.h>
#endif
#ifdef __SSE4_1__
#include <smmintrin.h>
#endif

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

int _tableCompare(const SHVar &a, const SHVar &b);
int _seqCompare(const SHVar &a, const SHVar &b);

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
    // Scalar is faster for just 2 elements
    const int64_t *av = (const int64_t *)&a.payload.int2Value;
    const int64_t *bv = (const int64_t *)&b.payload.int2Value;
    return av[0] == bv[0] && av[1] == bv[1];
  }
  case SHType::Int3: {
    // Scalar is faster for just 3 elements
    const int32_t *av = (const int32_t *)&a.payload.int3Value;
    const int32_t *bv = (const int32_t *)&b.payload.int3Value;
    return av[0] == bv[0] && av[1] == bv[1] && av[2] == bv[2];
  }
  case SHType::Int4: {
#ifdef __ARM_NEON
    auto va = vld1q_s32((const int32_t *)&a.payload.int4Value);
    auto vb = vld1q_s32((const int32_t *)&b.payload.int4Value);
    auto cmp = vceqq_s32(va, vb);
    return vminvq_u32(cmp) == 0xFFFFFFFF;
#elif defined(__SSE2__)
    auto va = _mm_load_si128((const __m128i *)&a.payload.int4Value);
    auto vb = _mm_load_si128((const __m128i *)&b.payload.int4Value);
    auto cmp = _mm_cmpeq_epi32(va, vb);
    return _mm_movemask_epi8(cmp) == 0xFFFF;
#else
    const int32_t *av = (const int32_t *)&a.payload.int4Value;
    const int32_t *bv = (const int32_t *)&b.payload.int4Value;
    return av[0] == bv[0] && av[1] == bv[1] && av[2] == bv[2] && av[3] == bv[3];
#endif
  }
  case SHType::Int8: {
#ifdef __ARM_NEON
    auto va = vld1q_s16((const int16_t *)&a.payload.int8Value);
    auto vb = vld1q_s16((const int16_t *)&b.payload.int8Value);
    auto cmp = vceqq_s16(va, vb);
    return vminvq_u16(cmp) == 0xFFFF;
#elif defined(__SSE2__)
    auto va = _mm_load_si128((const __m128i *)&a.payload.int8Value);
    auto vb = _mm_load_si128((const __m128i *)&b.payload.int8Value);
    auto cmp = _mm_cmpeq_epi16(va, vb);
    return _mm_movemask_epi8(cmp) == 0xFFFF;
#else
    const int16_t *av = (const int16_t *)&a.payload.int8Value;
    const int16_t *bv = (const int16_t *)&b.payload.int8Value;
    return av[0] == bv[0] && av[1] == bv[1] && av[2] == bv[2] && av[3] == bv[3] && av[4] == bv[4] && av[5] == bv[5] &&
           av[6] == bv[6] && av[7] == bv[7];
#endif
  }
  case SHType::Int16: {
#ifdef __ARM_NEON
    auto va = vld1q_s8((const int8_t *)&a.payload.int16Value);
    auto vb = vld1q_s8((const int8_t *)&b.payload.int16Value);
    auto cmp = vceqq_s8(va, vb);
    return vminvq_u8(cmp) == 0xFF;
#elif defined(__SSE2__)
    auto va = _mm_load_si128((const __m128i *)&a.payload.int16Value);
    auto vb = _mm_load_si128((const __m128i *)&b.payload.int16Value);
    auto cmp = _mm_cmpeq_epi8(va, vb);
    return _mm_movemask_epi8(cmp) == 0xFFFF;
#else
    // For 16 bytes, memcmp is likely optimal
    return memcmp(&a.payload.int16Value, &b.payload.int16Value, 16) == 0;
#endif
  }
  case SHType::Float2: {
    // Scalar is faster for just 2 elements - use appropriate epsilon for data type
    const double *av = (const double *)&a.payload.float2Value;
    const double *bv = (const double *)&b.payload.float2Value;
    return (fabs(av[0] - bv[0]) <= DBL_EPSILON) && (fabs(av[1] - bv[1]) <= DBL_EPSILON);
  }
  case SHType::Float3: {
    // Use SIMD for 4 elements, mask to check only first 3
#ifdef __ARM_NEON
    auto va = vld1q_f32((const float *)&a.payload.float3Value);
    auto vb = vld1q_f32((const float *)&b.payload.float3Value);
    auto veps = vdupq_n_f32(FLT_EPSILON);
    auto diff = vabsq_f32(vsubq_f32(va, vb));
    auto cmp = vcleq_f32(diff, veps);
    // Set 4th lane to "pass" value, then use horizontal min
    cmp = vsetq_lane_u32(0xFFFFFFFF, cmp, 3);
    return vminvq_u32(cmp) == 0xFFFFFFFF;
#elif defined(__SSE2__)
    auto va = _mm_load_ps((const float *)&a.payload.float3Value);
    auto vb = _mm_load_ps((const float *)&b.payload.float3Value);
    auto veps = _mm_set1_ps(FLT_EPSILON);
    auto diff = _mm_sub_ps(va, vb);
    auto sign_mask = _mm_set1_ps(-0.0f);
    diff = _mm_andnot_ps(sign_mask, diff);
    auto cmp = _mm_cmple_ps(diff, veps);
    auto mask = _mm_movemask_ps(cmp);
    return (mask & 0x7) == 0x7; // First 3 bits set
#else
    const float *av = (const float *)&a.payload.float3Value;
    const float *bv = (const float *)&b.payload.float3Value;
    return (fabsf(av[0] - bv[0]) <= FLT_EPSILON) && (fabsf(av[1] - bv[1]) <= FLT_EPSILON) &&
           (fabsf(av[2] - bv[2]) <= FLT_EPSILON);
#endif
  }
  case SHType::Float4: {
    // SIMD is actually beneficial for 4 floats
#ifdef __ARM_NEON
    auto va = vld1q_f32((const float *)&a.payload.float4Value);
    auto vb = vld1q_f32((const float *)&b.payload.float4Value);
    auto veps = vdupq_n_f32(FLT_EPSILON);
    auto diff = vabsq_f32(vsubq_f32(va, vb));
    auto cmp = vcleq_f32(diff, veps);
    return vminvq_u32(cmp) == 0xFFFFFFFF;
#elif defined(__SSE2__)
    auto va = _mm_load_ps((const float *)&a.payload.float4Value);
    auto vb = _mm_load_ps((const float *)&b.payload.float4Value);
    auto veps = _mm_set1_ps(FLT_EPSILON);
    auto diff = _mm_sub_ps(va, vb);
    // Manual abs for floats (clear sign bit)
    auto sign_mask = _mm_set1_ps(-0.0f);
    diff = _mm_andnot_ps(sign_mask, diff);
    auto cmp = _mm_cmple_ps(diff, veps);
    return _mm_movemask_ps(cmp) == 0xF;
#else
    const float *av = (const float *)&a.payload.float4Value;
    const float *bv = (const float *)&b.payload.float4Value;
    return (fabsf(av[0] - bv[0]) <= FLT_EPSILON) && (fabsf(av[1] - bv[1]) <= FLT_EPSILON) &&
           (fabsf(av[2] - bv[2]) <= FLT_EPSILON) && (fabsf(av[3] - bv[3]) <= FLT_EPSILON);
#endif
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
    if ((a.payload.imageValue->flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      apixsize = 2;
    else if ((a.payload.imageValue->flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      apixsize = 4;
    if ((b.payload.imageValue->flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      bpixsize = 2;
    else if ((b.payload.imageValue->flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      bpixsize = 4;
    return apixsize == bpixsize && a.payload.imageValue->channels == b.payload.imageValue->channels &&
           a.payload.imageValue->width == b.payload.imageValue->width &&
           a.payload.imageValue->height == b.payload.imageValue->height &&
           (a.payload.imageValue->data == b.payload.imageValue->data ||
            (memcmp(a.payload.imageValue->data, b.payload.imageValue->data,
                    a.payload.imageValue->channels * a.payload.imageValue->width * a.payload.imageValue->height * apixsize) ==
             0));
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
    return _seqCompare(a, b) == 0;
  case SHType::Table:
    return _tableCompare(a, b) == 0;
  case SHType::Bytes:
    return a.payload.bytesSize == b.payload.bytesSize &&
           (a.payload.bytesValue == b.payload.bytesValue ||
            memcmp(a.payload.bytesValue, b.payload.bytesValue, a.payload.bytesSize) == 0);
  case SHType::Trait:
    return memcmp(a.payload.traitValue->id, b.payload.traitValue->id, sizeof(SHTrait::id)) == 0;
  }

  return false;
}

// Vectorized comparison helpers
template <typename T, size_t N> ALWAYS_INLINE inline bool vector_less_than(const T *a, const T *b) {
  for (size_t i = 0; i < N; i++) {
    if (a[i] < b[i])
      return true;
    if (a[i] > b[i])
      return false;
  }
  return false;
}

template <typename T, size_t N> ALWAYS_INLINE inline bool vector_less_equal(const T *a, const T *b) {
  for (size_t i = 0; i < N; i++) {
    if (a[i] < b[i])
      return true;
    if (a[i] > b[i])
      return false;
  }
  return true;
}

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
    return vector_less_than<int64_t, 2>((const int64_t *)&a.payload.int2Value, (const int64_t *)&b.payload.int2Value);
  }
  case SHType::Int3: {
    return vector_less_than<int32_t, 3>((const int32_t *)&a.payload.int3Value, (const int32_t *)&b.payload.int3Value);
  }
  case SHType::Int4: {
    return vector_less_than<int32_t, 4>((const int32_t *)&a.payload.int4Value, (const int32_t *)&b.payload.int4Value);
  }
  case SHType::Int8: {
    return vector_less_than<int16_t, 8>((const int16_t *)&a.payload.int8Value, (const int16_t *)&b.payload.int8Value);
  }
  case SHType::Int16: {
    return vector_less_than<int8_t, 16>((const int8_t *)&a.payload.int16Value, (const int8_t *)&b.payload.int16Value);
  }
  case SHType::Float2: {
    return vector_less_than<double, 2>((const double *)&a.payload.float2Value, (const double *)&b.payload.float2Value);
  }
  case SHType::Float3: {
    return vector_less_than<float, 3>((const float *)&a.payload.float3Value, (const float *)&b.payload.float3Value);
  }
  case SHType::Float4: {
    return vector_less_than<float, 4>((const float *)&a.payload.float4Value, (const float *)&b.payload.float4Value);
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
    return a.payload.imageValue->data < b.payload.imageValue->data;
  case SHType::Seq:
    return _seqCompare(a, b) < 0;
  case SHType::Table:
    return _tableCompare(a, b) < 0;
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
  case SHType::EndOfBlittableTypes:
    shassert("Invalid type");
    return false;
  }

  return false;
}

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
    return vector_less_equal<int64_t, 2>((const int64_t *)&a.payload.int2Value, (const int64_t *)&b.payload.int2Value);
  }
  case SHType::Int3: {
    return vector_less_equal<int32_t, 3>((const int32_t *)&a.payload.int3Value, (const int32_t *)&b.payload.int3Value);
  }
  case SHType::Int4: {
    return vector_less_equal<int32_t, 4>((const int32_t *)&a.payload.int4Value, (const int32_t *)&b.payload.int4Value);
  }
  case SHType::Int8: {
    return vector_less_equal<int16_t, 8>((const int16_t *)&a.payload.int8Value, (const int16_t *)&b.payload.int8Value);
  }
  case SHType::Int16: {
    return vector_less_equal<int8_t, 16>((const int8_t *)&a.payload.int16Value, (const int8_t *)&b.payload.int16Value);
  }
  case SHType::Float2: {
    return vector_less_equal<double, 2>((const double *)&a.payload.float2Value, (const double *)&b.payload.float2Value);
  }
  case SHType::Float3: {
    return vector_less_equal<float, 3>((const float *)&a.payload.float3Value, (const float *)&b.payload.float3Value);
  }
  case SHType::Float4: {
    return vector_less_equal<float, 4>((const float *)&a.payload.float4Value, (const float *)&b.payload.float4Value);
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
    return a.payload.imageValue->data <= b.payload.imageValue->data;
  case SHType::Seq:
    return _seqCompare(a, b) <= 0;
  case SHType::Table:
    return _tableCompare(a, b) <= 0;
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
  case SHType::EndOfBlittableTypes:
    shassert("Invalid type");
    return false;
  case SHType::None:
    return true;
  case SHType::Any:
    return true;
  }
  __builtin_unreachable();
}

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
inline bool isSequenceOf(const SHTypeInfo &baseType, const SHTypeInfo &seqType, bool exclusive) {
  if (seqType.basicType != SHType::Seq) {
    return false;
  }

  if (exclusive) {
    return seqType.seqTypes.len == 1 && seqType.seqTypes.elements[0] == baseType;
  }

  bool match = false;
  for (uint32_t i = 0; i < seqType.seqTypes.len; i++) {
    if (seqType.seqTypes.elements[i] == baseType) {
      match = true;
    }
  }
  return match;
}
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

template <> struct less<SHExposedTypeInfo> {
  bool operator()(const SHExposedTypeInfo &lhs, const SHExposedTypeInfo &rhs) const {
    return std::string_view(lhs.name) < std::string_view(rhs.name);
  }
};

} // namespace std

#endif

/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#ifndef SH_LINALG_SHIM_HPP
#define SH_LINALG_SHIM_HPP

#include "shards.hpp"
#include "math_ops.hpp"
#include <linalg.h>
#include <vector>

namespace shards {
namespace padded {
template <typename T> struct _TypeOf {};
template <> struct _TypeOf<double> {
  static constexpr SHType type = SHType::Float;
};
template <> struct _TypeOf<linalg::vec<double, 2>> {
  static constexpr SHType type = SHType::Float2;
};
template <> struct _TypeOf<linalg::vec<float, 3>> {
  static constexpr SHType type = SHType::Float3;
};
template <> struct _TypeOf<linalg::vec<float, 4>> {
  static constexpr SHType type = SHType::Float4;
};
template <> struct _TypeOf<int64_t> {
  static constexpr SHType type = SHType::Int;
};
template <> struct _TypeOf<linalg::vec<int64_t, 2>> {
  static constexpr SHType type = SHType::Int2;
};
template <> struct _TypeOf<linalg::vec<int32_t, 3>> {
  static constexpr SHType type = SHType::Int3;
};
template <> struct _TypeOf<linalg::vec<int32_t, 4>> {
  static constexpr SHType type = SHType::Int4;
};

template <typename T> struct generic {
  using Self = generic<T>;
  using Base = T;

  T payload;
  uint8_t padding[16 - sizeof(T)];
  uint64_t _internal{};
  SHType valueType = _TypeOf<Self>::type;
  uint16_t flags{};
  uint32_t refcount{};

  generic() = default;
  generic(const Base &other) { *this = other; }
  generic(const Self &other) { *this = other; }

  operator const SHVar &() const { return reinterpret_cast<SHVar &>(*this); }
  operator SHVar &() { return reinterpret_cast<SHVar &>(*this); }
  operator const T &() const { return payload; }
  operator T &() { return payload; }
  const T &operator*() const { return payload; }
  T &operator*() { return payload; }
  Self &operator=(const T &other) {
    payload = other;
    return *this;
  }
  Self &operator=(const generic &other) {
    payload = other.payload;
    return *this;
  }
};
template <typename T, int M> struct vec {
  using Self = vec<T, M>;
  using TVec = linalg::vec<T, M>;
  using Base = TVec;

  TVec payload;
  uint8_t padding[16 - sizeof(TVec)];
  uint64_t _internal{};
  SHType valueType = _TypeOf<TVec>::type;
  uint16_t flags{};
  uint32_t refcount{};

  vec() = default;
  vec(const TVec &other) { *this = other; }
  vec(const Self &other) { *this = other; }

  operator const SHVar &() const { return reinterpret_cast<SHVar &>(*this); }
  operator SHVar &() { return reinterpret_cast<SHVar &>(*this); }
  operator const TVec &() const { return payload; }
  operator TVec &() { return payload; }
  const TVec &operator*() const { return payload; }
  TVec &operator*() { return payload; }
  Self &operator=(const TVec &other) {
    payload = other;
    return *this;
  }
  Self &operator=(const Self &other) {
    payload = other.payload;
    return *this;
  }
  const T &operator[](int i) const { return payload[i]; }
  T &operator[](int i) { return payload[i]; }
} __attribute__((aligned(16)));

using Float4 = vec<float, 4>;
using Float3 = vec<float, 3>;
using Float2 = vec<double, 2>;
using Float = generic<double>;

using Int4 = vec<int32_t, 4>;
using Int3 = vec<int32_t, 3>;
using Int2 = vec<int64_t, 2>;
using UInt2 = vec<uint64_t, 2>;
using Int = generic<int64_t>;
} // namespace padded

struct alignas(16) Mat2 {
  using Base = linalg::aliases::double2x2;
  using V = padded::Float2;
  V x, y;

  Mat2() = default;
  Mat2(const Base &other) { *this = other; }

  operator Base() const { return Base(x, y); }
  inline Mat2 &operator=(const Base &mat) {
    x = mat.x;
    y = mat.y;
    return *this;
  }
};

struct alignas(16) Mat3 {
  using Base = linalg::aliases::float3x3;
  using V = padded::Float3;
  V x, y, z;

  Mat3() = default;
  Mat3(const Base &other) { *this = other; }

  operator Base() const { return Base(x, y, z); }

  inline Mat3 &operator=(const Base &mat) {
    x = mat.x;
    y = mat.y;
    z = mat.z;
    return *this;
  }
};

struct alignas(16) Mat4 {
  using Base = linalg::aliases::float4x4;
  using V = padded::Float4;
  V x, y, z, w;

  Mat4() = default;
  Mat4(const SHVar &var) { *this = var; }
  Mat4(const Base &other) { *this = other; }
  Mat4(const linalg::identity_t &id) { *this = id; }

  operator Base() const { return Base(x, y, z, w); }

  constexpr const V &operator[](int j) const { return j == 0 ? x : j == 1 ? y : j == 2 ? z : w; }
  LINALG_CONSTEXPR14 V &operator[](int j) { return j == 0 ? x : j == 1 ? y : j == 2 ? z : w; }

  template <typename NUMBER> static Mat4 FromVector(const std::vector<NUMBER> &mat) {
    // used by gltf
    assert(mat.size() == 16);
    int idx = 0;
    Mat4 res;
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        res[i][j] = float(mat[idx]);
        idx++;
      }
    }
    return res;
  }

  template <typename NUMBER> static Mat4 FromArray(const std::array<NUMBER, 16> &mat) {
    // used by gltf
    assert(mat.size() == 16);
    int idx = 0;
    Mat4 res;
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        res[i][j] = float(mat[idx]);
        idx++;
      }
    }
    return res;
  }

  static Mat4 FromArrayUnsafe(const float *mat) {
    // used by gltf
    int idx = 0;
    Mat4 res;
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        res[i][j] = mat[idx];
        idx++;
      }
    }
    return res;
  }

  static void ToArrayUnsafe(const Mat4 &mat, float *array) {
    int idx = 0;
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        array[idx] = mat[i][j];
        idx++;
      }
    }
  }

  static Mat4 Identity() {
    Mat4 res;
    res[0] = {1, 0, 0, 0};
    res[1] = {0, 1, 0, 0};
    res[2] = {0, 0, 1, 0};
    res[3] = {0, 0, 0, 1};
    return res;
  }

  inline Mat4 &operator=(linalg::identity_t id) {
    *this = Identity();
    return *this;
  }

  inline Mat4 &operator=(const Base &mat) {
    (*this)[0] = mat[0];
    (*this)[1] = mat[1];
    (*this)[2] = mat[2];
    (*this)[3] = mat[3];
    return *this;
  }

  inline Mat4 &operator=(const SHVar &var) {
    if (var.valueType != SHType::Seq || var.payload.seqValue.len != 4)
      throw SHException("Invalid Mat4 variable given as input");
    auto vm = reinterpret_cast<const Mat4 *>(var.payload.seqValue.elements);
    (*this)[0] = (*vm)[0];
    (*this)[1] = (*vm)[1];
    (*this)[2] = (*vm)[2];
    (*this)[3] = (*vm)[3];
    return *this;
  }

  operator SHVar() const {
    SHVar res{};
    res.valueType = SHType::Seq;
    res.payload.seqValue.elements = reinterpret_cast<SHVar *>(const_cast<shards::Mat4 *>(this));
    res.payload.seqValue.len = 4;
    res.payload.seqValue.cap = 0;
    return res;
  }

  static Mat4 &fromVar(const SHVar &var) { return *reinterpret_cast<Mat4 *>(var.payload.seqValue.elements); }
};

using Vec2 = padded::Float2;
using Vec3 = padded::Float3;
using Vec4 = padded::Float4;

template <typename T> struct VectorConversion {};
template <> struct VectorConversion<float> {
  static inline constexpr SHType ShardsType = SHType::Float;
  static auto convert(const SHVar &value) { return value.payload.floatValue; }
  static SHVar rconvert(const float &value) { return SHVar{.payload = {.floatValue = value}, .valueType = ShardsType}; }
};
template <int N> struct VectorConversion<linalg::vec<float, N>> {
  static inline constexpr SHType ShardsType = SHType(int(SHType::Float) + N - 1);
  static auto convert(const SHVar &value) {
    static_assert(N <= 4, "Not implemented for N > 4");
    linalg::vec<float, N> r;
    for (int i = 0; i < N; i++)
      r[i] = Math::PayloadTraits<ShardsType>{}.getContents(const_cast<SHVarPayload &>(value.payload))[i];
    return r;
  }
  static SHVar rconvert(const linalg::vec<float, N> &v) {
    static_assert(N <= 4, "Not implemented for N > 4");
    SHVar r{};
    r.valueType = ShardsType;
    for (int i = 0; i < N; i++)
      Math::PayloadTraits<ShardsType>{}.getContents(const_cast<SHVarPayload &>(r.payload))[i] = v[i];
    return r;
  }
};

template <int N> struct VectorConversion<linalg::vec<int, N>> {
  static inline constexpr SHType ShardsType = SHType(int(SHType::Int) + N - 1);
  static auto convert(const SHVar &value) {
    static_assert(N <= 4, "Not implemented for N > 4");
    linalg::vec<int, N> r;
    for (int i = 0; i < N; i++)
      r[i] = int(Math::PayloadTraits<ShardsType>{}.getContents(const_cast<SHVarPayload &>(value.payload))[i]);
    return r;
  }
  static SHVar rconvert(const linalg::vec<int, N> &v) {
    static_assert(N <= 4, "Not implemented for N > 4");
    SHVar r{};
    r.valueType = ShardsType;
    for (int i = 0; i < N; i++)
      Math::PayloadTraits<ShardsType>{}.getContents(const_cast<SHVarPayload &>(r.payload))[i] = v[i];
    return r;
  }
};

template <typename TVec> inline auto toVec(const SHVar &value) {
  using Conv = VectorConversion<TVec>;
  if (value.valueType != Conv::ShardsType)
    throw std::runtime_error(fmt::format("Invalid vector type {}, expected {}", value.valueType, Conv::ShardsType));
  return Conv::convert(value);
}

inline auto toFloat2(const SHVar &value) { return *reinterpret_cast<const padded::Float2 &>(value); }
inline auto toFloat3(const SHVar &value) { return *reinterpret_cast<const padded::Float3 &>(value); }
inline auto toFloat4(const SHVar &value) { return *reinterpret_cast<const padded::Float4 &>(value); }
inline auto toInt2(const SHVar &value) { return *reinterpret_cast<const padded::Int2 &>(value); }
inline auto toUInt2(const SHVar &value) { return *reinterpret_cast<const padded::UInt2 &>(value); }
inline auto toInt3(const SHVar &value) { return *reinterpret_cast<const padded::Int3 &>(value); }
inline auto toInt4(const SHVar &value) { return *reinterpret_cast<const padded::Int4 &>(value); }

template <typename TVec> inline SHVar genericToVar(const TVec &value) {
  using Conv = VectorConversion<TVec>;
  return Conv::rconvert(value);
}

inline auto toVar(const linalg::aliases::float2 &value) { return genericToVar(value); }
inline auto toVar(const linalg::aliases::float3 &value) { return genericToVar(value); }
inline auto toVar(const linalg::aliases::float4 &value) { return genericToVar(value); }
inline auto toVar(const linalg::aliases::int2 &value) { return genericToVar(value); }
inline auto toVar(const linalg::aliases::int3 &value) { return genericToVar(value); }
inline auto toVar(const linalg::aliases::int4 &value) { return genericToVar(value); }

inline linalg::aliases::float4x4 toFloat4x4(const SHVar &vec) { return Mat4(vec); }

constexpr linalg::aliases::float3 AxisX{1.0, 0.0, 0.0};
constexpr linalg::aliases::float3 AxisY{0.0, 1.0, 0.0};
constexpr linalg::aliases::float3 AxisZ{0.0, 0.0, 1.0};

}; // namespace shards

#endif

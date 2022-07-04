/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#ifndef SH_LINALG_SHIM_HPP
#define SH_LINALG_SHIM_HPP

#include <linalg.h>
#include <vector>
namespace shards {
struct alignas(16) Mat4 : public linalg::aliases::float4x4 {
  using linalg::aliases::float4x4::mat;
  Mat4(const SHVar &var) { *this = var; }
  Mat4(const linalg::aliases::float4x4 &other) : linalg::aliases::float4x4(other) {}

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

  Mat4 &operator=(linalg::aliases::float4x4 &&mat) {
    (*this)[0] = std::move(mat[0]);
    (*this)[1] = std::move(mat[1]);
    (*this)[2] = std::move(mat[2]);
    (*this)[3] = std::move(mat[3]);
    return *this;
  }

  Mat4 &operator=(const SHVar &var) {
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
    for (auto i = 0; i < 4; i++) {
      res.payload.seqValue.elements[i].valueType = SHType::Float4;
    }
    return res;
  }
};

struct alignas(16) Vec2 : public linalg::aliases::float2 {
  using linalg::aliases::float2::vec;
  using linalg::aliases::float2::operator=;

  Vec2 &operator=(const SHVar &var);

  template <typename XY_TYPE> Vec2(const XY_TYPE &vec) {
    x = float(vec.x);
    y = float(vec.y);
  }

  template <typename NUMBER> Vec2(NUMBER x_, NUMBER y_) {
    x = float(x_);
    y = float(y_);
  }
};

struct alignas(16) Vec3 : public linalg::aliases::float3 {
  using linalg::aliases::float3::vec;
  using linalg::aliases::float3::operator=;

  Vec3 &operator=(const SHVar &var);

  template <typename XYZ_TYPE> Vec3(const XYZ_TYPE &vec) {
    x = float(vec.x);
    y = float(vec.y);
    z = float(vec.z);
  }

  template <typename NUMBER> Vec3(NUMBER x_, NUMBER y_, NUMBER z_) {
    x = float(x_);
    y = float(y_);
    z = float(z_);
  }

  template <typename NUMBER> static Vec3 FromVector(const std::vector<NUMBER> &vec) {
    // used by gltf
    assert(vec.size() == 3);
    Vec3 res;
    for (int j = 0; j < 3; j++) {
      res[j] = float(vec[j]);
    }
    return res;
  }

  Vec3 &applyMatrix(const linalg::aliases::float4x4 &mat) {
    const auto w = 1.0f / (mat.x.w * x + mat.y.w * y + mat.z.w * z + mat.w.w);
    x = (mat.x.x * x + mat.y.x * y + mat.z.x * z + mat.w.x) * w;
    y = (mat.x.y * x + mat.y.y * y + mat.z.y * z + mat.w.y) * w;
    z = (mat.x.z * x + mat.y.z * y + mat.z.z * z + mat.w.z) * w;
    return *this;
  }

  operator SHVar() const {
    auto v = reinterpret_cast<SHVar *>(const_cast<shards::Vec3 *>(this));
    v->valueType = SHType::Float3;
    return *v;
  }
};

struct alignas(16) Vec4 : public linalg::aliases::float4 {
  using linalg::aliases::float4::vec;
  using linalg::aliases::float4::operator=;

  template <typename XYZW_TYPE> Vec4(const XYZW_TYPE &vec) {
    x = float(vec.x);
    y = float(vec.y);
    z = float(vec.z);
    w = float(vec.w);
  }

  template <typename NUMBER> Vec4(NUMBER x_, NUMBER y_, NUMBER z_, NUMBER w_) {
    x = float(x_);
    y = float(y_);
    z = float(z_);
    w = float(w_);
  }

  Vec4 &operator=(const SHVar &var);

  constexpr static Vec4 Quaternion() {
    Vec4 q;
    q.x = 0.0;
    q.y = 0.0;
    q.z = 0.0;
    q.w = 1.0;
    return q;
  }

  template <typename NUMBER> static Vec4 FromVector(const std::vector<NUMBER> &vec) {
    // used by gltf
    assert(vec.size() == 4);
    Vec4 res;
    for (int j = 0; j < 4; j++) {
      res[j] = float(vec[j]);
    }
    return res;
  }

  Vec4 &operator=(const linalg::aliases::float4 &vec) {
    x = vec.x;
    y = vec.y;
    z = vec.z;
    w = vec.w;
    return *this;
  }

  operator SHVar() const {
    auto v = reinterpret_cast<SHVar *>(const_cast<shards::Vec4 *>(this));
    v->valueType = SHType::Float4;
    return *v;
  }
};

inline linalg::aliases::float2 toFloat2(const SHVar &vec) {
  if (vec.valueType != SHType::Float2)
    throw InvalidVarTypeError("Invalid variable casting! expected Float2");
  return linalg::aliases::float2(float(vec.payload.float2Value[0]), float(vec.payload.float2Value[1]));
}

inline linalg::aliases::float3 toFloat3(const SHVar &vec) {
  if (vec.valueType != SHType::Float3)
    throw InvalidVarTypeError("Invalid variable casting! expected Float3");
  return linalg::aliases::float3(float(vec.payload.float3Value[0]), float(vec.payload.float3Value[1]),
                                 float(vec.payload.float3Value[2]));
}

inline linalg::aliases::float4 toFloat4(const SHVar &vec) {
  if (vec.valueType != SHType::Float4)
    throw InvalidVarTypeError("Invalid variable casting! expected Float4");
  return linalg::aliases::float4(float(vec.payload.float4Value[0]), float(vec.payload.float4Value[1]),
                                 float(vec.payload.float4Value[2]), float(vec.payload.float4Value[3]));
}

inline linalg::aliases::float4x4 toFloat4x4(const SHVar &vec) { return Mat4(vec); }

inline SHVar toVar(const linalg::aliases::float2 &vec) {
  SHVar var{
      .valueType = SHType::Float2,
  };
  var.payload.float2Value[0] = vec.x;
  var.payload.float2Value[1] = vec.y;
  return var;
}

inline SHVar toVar(const linalg::aliases::float3 &vec) {
  SHVar var{
      .valueType = SHType::Float3,
  };
  var.payload.float3Value[0] = vec.x;
  var.payload.float3Value[1] = vec.y;
  var.payload.float3Value[2] = vec.z;
  return var;
}

inline SHVar toVar(const linalg::aliases::float4 &vec) {
  SHVar var{
      .valueType = SHType::Float4,
  };
  var.payload.float4Value[0] = vec.x;
  var.payload.float4Value[1] = vec.y;
  var.payload.float4Value[2] = vec.z;
  var.payload.float4Value[3] = vec.w;
  return var;
}

constexpr linalg::aliases::float3 AxisX{1.0, 0.0, 0.0};
constexpr linalg::aliases::float3 AxisY{0.0, 1.0, 0.0};
constexpr linalg::aliases::float3 AxisZ{0.0, 0.0, 1.0};

}; // namespace shards

#endif

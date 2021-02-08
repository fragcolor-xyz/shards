/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#ifndef CB_LINALG_SHIM_HPP
#define CB_LINALG_SHIM_HPP

#include <linalg.h>
#include <vector>
namespace chainblocks {
struct alignas(16) Mat4 : public linalg::aliases::float4x4 {
  template <typename NUMBER>
  static Mat4 FromVector(const std::vector<NUMBER> &mat) {
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

  template <typename NUMBER>
  static Mat4 FromArray(const std::array<NUMBER, 16> &mat) {
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

  Mat4 &operator=(linalg::aliases::float4x4 &&mat) {
    (*this)[0] = std::move(mat[0]);
    (*this)[1] = std::move(mat[1]);
    (*this)[2] = std::move(mat[2]);
    (*this)[3] = std::move(mat[3]);
    return *this;
  }

  operator CBVar() const {
    CBVar res{};
    res.valueType = CBType::Seq;
    res.payload.seqValue.elements =
        reinterpret_cast<CBVar *>(const_cast<chainblocks::Mat4 *>(this));
    res.payload.seqValue.len = 4;
    res.payload.seqValue.cap = 0;
    for (auto i = 0; i < 4; i++) {
      res.payload.seqValue.elements[i].valueType = CBType::Float4;
    }
    return res;
  }
};

struct alignas(16) Vec4 : public linalg::aliases::float4 {
  constexpr static Vec4 Quaternion() {
    Vec4 q;
    q.x = 0.0;
    q.y = 0.0;
    q.z = 0.0;
    q.w = 1.0;
    return q;
  }

  template <typename NUMBER>
  static Vec4 FromVector(const std::vector<NUMBER> &vec) {
    // used by gltf
    assert(vec.size() == 4);
    Vec4 res;
    for (int j = 0; j < 4; j++) {
      res[j] = float(vec[j]);
    }
    return res;
  }

  operator CBVar() const {
    auto v = reinterpret_cast<CBVar *>(const_cast<chainblocks::Vec4 *>(this));
    v->valueType = CBType::Float4;
    return *v;
  }
};

struct alignas(16) Vec3 : public linalg::aliases::float3 {
  Vec3() : linalg::aliases::float3() {}

  template <typename NUMBER> Vec3(NUMBER x_, NUMBER y_, NUMBER z_) {
    x = float(x_);
    y = float(y_);
    z = float(z_);
  }

  template <typename NUMBER>
  static Vec3 FromVector(const std::vector<NUMBER> &vec) {
    // used by gltf
    assert(vec.size() == 3);
    Vec3 res;
    for (int j = 0; j < 3; j++) {
      res[j] = float(vec[j]);
    }
    return res;
  }

  operator CBVar() const {
    auto v = reinterpret_cast<CBVar *>(const_cast<chainblocks::Vec3 *>(this));
    v->valueType = CBType::Float3;
    return *v;
  }
};
}; // namespace chainblocks

#endif

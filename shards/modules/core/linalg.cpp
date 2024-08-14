/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "linalg.hpp"
#include <shards/core/module.hpp>
#include <shards/core/foundation.hpp>
#include <shards/linalg_shim.hpp>
#include <shards/math_ops.hpp>
#include <linalg.h>
#include <shards/gfx/linalg.hpp>

// Ok...
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

namespace shards {
namespace Math {
namespace LinAlg {

void CrossOp::apply(SHVar &output, const SHVar &input, const SHVar &operand) {
  if (operand.valueType != SHType::Float3)
    throw ActivationError("LinAlg.Cross works only with SHType::Float3 types.");

  switch (input.valueType) {
  case SHType::Float3: {
    const SHInt3 mask1 = {1, 2, 0};
    const SHInt3 mask2 = {2, 0, 1};
    auto a1 = shufflevector(input.payload.float3Value, mask1);
    auto a2 = shufflevector(input.payload.float3Value, mask2);
    auto b1 = shufflevector(operand.payload.float3Value, mask1);
    auto b2 = shufflevector(operand.payload.float3Value, mask2);
    output.valueType = SHType::Float3;
    output.payload.float3Value = a1 * b2 - a2 * b1;
  } break;
  default:
    throw ActivationError("LinAlg.Cross works only with SHType::Float3 types.");
  }
}

void DotOp::apply(SHVar &output, const SHVar &input, const SHVar &operand) {
  if (operand.valueType != input.valueType)
    throw ActivationError("LinAlg.Dot works only with same input and operand types.");

  switch (input.valueType) {
  case SHType::Float2: {
    output.valueType = SHType::Float;
    output.payload.floatValue = input.payload.float2Value[0] * operand.payload.float2Value[0];
    output.payload.floatValue += input.payload.float2Value[1] * operand.payload.float2Value[1];
  } break;
  case SHType::Float3: {
    output.valueType = SHType::Float;
    output.payload.floatValue = input.payload.float3Value[0] * operand.payload.float3Value[0];
    output.payload.floatValue += input.payload.float3Value[1] * operand.payload.float3Value[1];
    output.payload.floatValue += input.payload.float3Value[2] * operand.payload.float3Value[2];
  } break;
  case SHType::Float4: {
    output.valueType = SHType::Float;
    output.payload.floatValue = input.payload.float4Value[0] * operand.payload.float4Value[0];
    output.payload.floatValue += input.payload.float4Value[1] * operand.payload.float4Value[1];
    output.payload.floatValue += input.payload.float4Value[2] * operand.payload.float4Value[2];
    output.payload.floatValue += input.payload.float4Value[3] * operand.payload.float4Value[3];
  } break;
  default:
    break;
  }
}

void NormalizeOp::apply(SHVar &output, const SHVar &input) {
  SHVar len{};
  lenOp.apply(len, input);

  switch (input.valueType) {
  case SHType::Float2: {
    output.valueType = SHType::Float2;
    if (len.payload.floatValue > 0 || !positiveOnly) {
      SHFloat2 vlen = {len.payload.floatValue, len.payload.floatValue};
      output.payload.float2Value = input.payload.float2Value / vlen;
      if (positiveOnly) {
        output.payload.float2Value += 1.0;
        output.payload.float2Value /= 2.0;
      }
    } else {
      output.payload.float2Value = input.payload.float2Value;
    }
  } break;
  case SHType::Float3: {
    output.valueType = SHType::Float3;
    if (len.payload.floatValue > 0 || !positiveOnly) {
      SHFloat3 vlen = {float(len.payload.floatValue), float(len.payload.floatValue), float(len.payload.floatValue)};
      output.payload.float3Value = input.payload.float3Value / vlen;
      if (positiveOnly) {
        output.payload.float3Value += 1.0;
        output.payload.float3Value /= 2.0;
      }
    } else {
      output.payload.float3Value = input.payload.float3Value;
    }
  } break;
  case SHType::Float4: {
    output.valueType = SHType::Float4;
    if (len.payload.floatValue > 0 || !positiveOnly) {
      SHFloat4 vlen = {float(len.payload.floatValue), float(len.payload.floatValue), float(len.payload.floatValue),
                       float(len.payload.floatValue)};
      output.payload.float4Value = input.payload.float4Value / vlen;
      if (positiveOnly) {
        output.payload.float4Value += 1.0;
        output.payload.float4Value /= 2.0;
      }
    } else {
      output.payload.float4Value = input.payload.float4Value;
    }
  } break;
  default:
    break;
  }
}

SHVar Normalize::activateFloatSeq(SHContext *context, const SHVar &input) {
  const bool positiveOnly = op.op.positiveOnly;
  auto &inputSeq = input.payload.seqValue;
  const auto len = inputSeq.len;

  if (_result.valueType != SHType::Seq)
    destroyVar(_result);

  auto &outputSeq = _result.payload.seqValue;
  if (_result.valueType != SHType::Seq || outputSeq.len != len) {
    _result.valueType = SHType::Seq;
    shards::arrayResize(outputSeq, len);
    for (uint32_t i = 0; i < len; i++) {
      outputSeq.elements[i].valueType = SHType::Float;
    }
  }

  float vlen = 0.0;
  for (uint32_t i = 0; i < len; i++) {
    const auto f = inputSeq.elements[i].payload.floatValue;
    vlen += f * f;
  }
  vlen = __builtin_sqrt(vlen);
  if (vlen > 0 || !positiveOnly) {
    // better branching here
    if (!positiveOnly) {
      for (uint32_t i = 0; i < len; i++) {
        const auto f = inputSeq.elements[i].payload.floatValue;
        outputSeq.elements[i].payload.floatValue = f / vlen;
      }
    } else {
      for (uint32_t i = 0; i < len; i++) {
        const auto f = inputSeq.elements[i].payload.floatValue;
        outputSeq.elements[i].payload.floatValue = ((f / vlen) + 1.0) / 2.0;
      }
    }
    return Var(_result);
  } else {
    return input;
  }
}

SHVar MatMul::activate(SHContext *context, const SHVar &input) {
  auto &operand = _operand.get();
  // expect SeqSeq as in 2x 2D arrays or SHType::Seq1 Mat @ Vec
  if (_opType == SeqSeq) {
#define MATMUL_OP(_v1_, _v2_, _n_)                                          \
  shards::arrayResize(_result.payload.seqValue, _n_);                       \
  auto &a = reinterpret_cast<_v1_ &>(input.payload.seqValue.elements[0]);   \
  auto &b = reinterpret_cast<_v1_ &>(operand.payload.seqValue.elements[0]); \
  auto &c = reinterpret_cast<_v1_ &>(_result.payload.seqValue.elements[0]); \
  c = linalg::mul((_v1_::Base)a, (_v1_::Base)b);                            \
  for (auto i = 0; i < _n_; i++) {                                          \
    _result.payload.seqValue.elements[i].valueType = _v2_;                  \
  }
    _result.valueType = SHType::Seq;
    switch (input.payload.seqValue.elements[0].valueType) {
    case SHType::Float2: {
      MATMUL_OP(Mat2, SHType::Float2, 2);
    } break;
    case SHType::Float3: {
      MATMUL_OP(Mat3, SHType::Float3, 3);
    } break;
    case SHType::Float4: {
      MATMUL_OP(Mat4, SHType::Float4, 4);
    } break;
    default:
      throw ActivationError("Invalid value type for MatMul");
    }
#undef MATMUL_OP
    return _result;
  } else if (_opType == Seq1) {
#define MATMUL_OP(_v1_, _v2_, _n_, _v3_, _v3v_)                           \
  auto &a = reinterpret_cast<_v1_ &>(input.payload.seqValue.elements[0]); \
  auto &b = reinterpret_cast<_v3_ &>(operand.payload._v3v_);              \
  auto &c = reinterpret_cast<_v3_ &>(_result.payload._v3v_);              \
  c = linalg::mul((_v1_::Base)a, (_v3_::Base)b);                          \
  _result.valueType = _v2_;
    switch (input.payload.seqValue.elements[0].valueType) {
    case SHType::Float2: {
      MATMUL_OP(Mat2, SHType::Float2, 2, Vec2, float2Value);
    } break;
    case SHType::Float3: {
      MATMUL_OP(Mat3, SHType::Float3, 3, Vec3, float3Value);
    } break;
    case SHType::Float4: {
      MATMUL_OP(Mat4, SHType::Float4, 4, Vec4, float4Value);
    } break;
    default:
      throw ActivationError("Invalid value type for MatMul");
    }
    return _result;
  } else {
    throw ActivationError("MatMul expects either Mat (SHType::Seq of FloatX) @ Mat or "
                          "Mat @ Vec (FloatX)");
  }
}

SHVar Transpose::activate(SHContext *context, const SHVar &input) {
  size_t height = input.payload.seqValue.len;
  if (height < 2 || height > 4) {
    // todo 2x1 should be go too
    throw ActivationError("Transpose expects a 2x2 to 4x4 matrix array.");
  }

  size_t width = 0;
  switch (input.payload.seqValue.elements[0].valueType) {
  case SHType::Float2:
    width = 2;
    break;
  case SHType::Float3:
    width = 3;
    break;
  case SHType::Float4:
    width = 4;
    break;
  default:
    break;
  }

  _result.valueType = SHType::Seq;
  shards::arrayResize(_result.payload.seqValue, width);

  double v1{}, v2{}, v3{}, v4{};
  for (size_t w = 0; w < width; w++) {
    switch (height) {
    case 2:
      _result.payload.seqValue.elements[w].valueType = SHType::Float2;
      switch (width) {
      case 2:
        v1 = input.payload.seqValue.elements[0].payload.float2Value[w];
        v2 = input.payload.seqValue.elements[1].payload.float2Value[w];
        break;
      case 3:
        v1 = input.payload.seqValue.elements[0].payload.float3Value[w];
        v2 = input.payload.seqValue.elements[1].payload.float3Value[w];
        break;
      case 4:
        v1 = input.payload.seqValue.elements[0].payload.float4Value[w];
        v2 = input.payload.seqValue.elements[1].payload.float4Value[w];
        break;
      default:
        break;
      }
      _result.payload.seqValue.elements[w].payload.float2Value[0] = v1;
      _result.payload.seqValue.elements[w].payload.float2Value[1] = v2;
      break;
    case 3:
      _result.payload.seqValue.elements[w].valueType = SHType::Float3;
      switch (width) {
      case 2:
        v1 = input.payload.seqValue.elements[0].payload.float2Value[w];
        v2 = input.payload.seqValue.elements[1].payload.float2Value[w];
        v3 = input.payload.seqValue.elements[2].payload.float2Value[w];
        break;
      case 3:
        v1 = input.payload.seqValue.elements[0].payload.float3Value[w];
        v2 = input.payload.seqValue.elements[1].payload.float3Value[w];
        v3 = input.payload.seqValue.elements[2].payload.float3Value[w];
        break;
      case 4:
        v1 = input.payload.seqValue.elements[0].payload.float4Value[w];
        v2 = input.payload.seqValue.elements[1].payload.float4Value[w];
        v3 = input.payload.seqValue.elements[2].payload.float4Value[w];
        break;
      default:
        break;
      }
      _result.payload.seqValue.elements[w].payload.float3Value[0] = v1;
      _result.payload.seqValue.elements[w].payload.float3Value[1] = v2;
      _result.payload.seqValue.elements[w].payload.float3Value[2] = v3;
      break;
    case 4:
      _result.payload.seqValue.elements[w].valueType = SHType::Float4;
      switch (width) {
      case 2:
        v1 = input.payload.seqValue.elements[0].payload.float2Value[w];
        v2 = input.payload.seqValue.elements[1].payload.float2Value[w];
        v3 = input.payload.seqValue.elements[2].payload.float2Value[w];
        v4 = input.payload.seqValue.elements[3].payload.float2Value[w];
        break;
      case 3:
        v1 = input.payload.seqValue.elements[0].payload.float3Value[w];
        v2 = input.payload.seqValue.elements[1].payload.float3Value[w];
        v3 = input.payload.seqValue.elements[2].payload.float3Value[w];
        v4 = input.payload.seqValue.elements[3].payload.float3Value[w];
        break;
      case 4:
        v1 = input.payload.seqValue.elements[0].payload.float4Value[w];
        v2 = input.payload.seqValue.elements[1].payload.float4Value[w];
        v3 = input.payload.seqValue.elements[2].payload.float4Value[w];
        v4 = input.payload.seqValue.elements[3].payload.float4Value[w];
        break;
      default:
        break;
      }
      _result.payload.seqValue.elements[w].payload.float4Value[0] = v1;
      _result.payload.seqValue.elements[w].payload.float4Value[1] = v2;
      _result.payload.seqValue.elements[w].payload.float4Value[2] = v3;
      _result.payload.seqValue.elements[w].payload.float4Value[3] = v4;
      break;
    }
  }

  return _result;
}

SHVar Inverse::activate(SHContext *context, const SHVar &input) {
  size_t height = input.payload.seqValue.len;
  size_t width = 0;
  const SHType rowType = input.payload.seqValue.elements[0].valueType;
  switch (rowType) {
  case SHType::Float2:
    width = 2;
    break;
  case SHType::Float3:
    width = 3;
    break;
  case SHType::Float4:
    width = 4;
    break;
  default:
    break;
  }

  if (height != width && width != 4) {
    throw ActivationError("Inverse expects a 4x4 matrix.");
  }
  Mat4 inverted(linalg::inverse(toFloat4x4(input)));

  _result.valueType = SHType::Seq;
  shards::arrayResize(_result.payload.seqValue, width);

  for (size_t i = 0; i < height; i++) {
    linalg::aliases::float4 &srcRow = inverted[i];
    SHVar &dstRow = _result.payload.seqValue.elements[i];
    dstRow.valueType = rowType;
    for (size_t j = 0; j < width; j++) {
      dstRow.payload.float4Value[j] = srcRow[j];
    }
  }

  return _result;
}

struct Decompose {
  TableVar output{};

  static SHTypesInfo inputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a 4x4 transformation matrix as input. This matrix should be a sequence of four float4 vectors "
                   "representing the combined translation, rotation, and scale transformations.");
  }
  static SHTypesInfo outputTypes() { return Types::TRS; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns a table containing the Translation, Rotation, and Scale components. Eg. {translation: @f3(1 2 3), "
                   "rotation: @f4(0 0 0 1), scale: @f3(1 1 1)}");
  }

  static SHOptionalString help() {
    return SHCCSTR("This shard converts a 4x4 transformation matrix (a sequence of four float 4 vectors) into a table containing "
                   "its constituent Translation, Rotation, and Scale components. "
                   "The table has a Translation key with a float3 vector value representing positions on the x, y, z axes, a "
                   "Rotation key with a float4 vector value representing the quaternion rotation, and a Scale key with a float3 "
                   "vector value, representing the size on the x, y, z axes. Eg. {translation: @f3(1 2 3), rotation: @f4(0 0 0 "
                   "1), scale: @f3(1 1 1)} "
                   "A float3 vector is a vector with 3 float elements while a float4 vector is a vector with 4 float elements. ");
  }

  static inline Var v_scale{"scale"};
  static inline Var v_translation{"translation"};
  static inline Var v_rotation{"rotation"};

  SHVar activate(SHContext *context, const SHVar &input) {
    using namespace linalg::aliases;
    Mat4 mat = toFloat4x4(input);
    float3 scale;
    float3x3 rotation;
    float3 translation;
    gfx::decomposeTRS(mat, translation, scale, rotation);
    float4 qRot = linalg::rotation_quat(rotation);
    output[Var("translation")] = toVar(translation);
    output[Var("rotation")] = toVar(qRot);
    output[Var("scale")] = toVar(scale);
    return output;
  }
};

struct Compose {
  static SHTypesInfo inputTypes() { return Types::TRS; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a table as input. The table should have a Translation key with a float3 vector value, a Rotation key "
                   "with a float4 vector value and a Scale key with a float3 vector value. Eg. {translation: @f3(1 2 3), "
                   "rotation: @f4(0 0 0 1), scale: @f3(1 1 1)}");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns a 4x4 transformation matrix (sequence of four float4 vectors) "
                   "that combines the input translation, rotation, and scale.");
  }

  static SHOptionalString help() {
    return SHCCSTR(
        "Creates a 4x4 transformation matrix (sequence of four float4 vectors) from a table containing the appropriate "
        "Translation, Rotation and Scale values. "
        "values. The translation value should be a float3 vector representing positions on the x y z axis. The "
        "rotation value should be a float4 vector representing the quaternion rotation. Lastly, the scale should be a "
        "float3 vector representing the size on the x y and z axis. Eg. {translation: @f3(1 2 3), rotation: @f4(0 0 0 1), scale: "
        "@f3(1 1 1)} "
        "A float3 vector is a vector with 3 float elements while a float4 vector is a vector with 4 float elements.");
  }

  Mat4 _output;

  SHVar activate(SHContext *context, const SHVar &input_) {
    using namespace linalg::aliases;
    TableVar &input = (TableVar &)input_;
    Vec3 scale = reinterpret_cast<Vec3 &>(input[Decompose::v_scale]);
    Vec3 translation = reinterpret_cast<Vec3 &>(input[Decompose::v_translation]);
    Vec4 rotation = reinterpret_cast<Vec4 &>(input[Decompose::v_rotation]);
    auto t = linalg::translation_matrix(translation.payload);
    auto r = linalg::rotation_matrix(rotation.payload);
    auto s = linalg::scaling_matrix(scale.payload);
    _output = linalg::mul(linalg::mul(t, r), s);
    return _output;
  }
};

struct Project {
  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a float3 vector representing the 3D point in world space where x, y, and z are the coordinates in world space.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns a float3 vector representing the projected 2D point (x, y) in screen space, "
                   "with the z component representing the depth.");
  }

  static SHOptionalString help() {
    return SHCCSTR("This shard converts the input 3D world coordinates to 2D screen coordinates "
                   "using a view-projection matrix. Both 3D and 2D coordinates are represented as float3 vectors (vectors with 3 "
                   "float elements)."
                   "It performs the full projection pipeline including matrix multiplication, "
                   "perspective division, and viewport transformation using the 4x4 view-projection matrix specified in the "
                   "Matrix parameter and the screen size in the ScreenSize parameter.");
  }

  PARAM_PARAMVAR(_mtx, "Matrix", "The combined 4x4 view-projection matrix (sequence of four float4 vectors) to use.",
                 {CoreInfo::Float4x4Type, Type::VariableOf(CoreInfo::Float4x4Type)});
  PARAM_PARAMVAR(_screenSize, "ScreenSize", "The size of the screen or viewport in pixels.",
                 {CoreInfo::Float2Type, Type::VariableOf(CoreInfo::Float2Type)});
  PARAM_PARAMVAR(_flipY, "FlipY", "Flip Y coordinate (on by default).",
                 {CoreInfo::BoolType, Type::VariableOf(CoreInfo::BoolVarType)});
  PARAM_IMPL(PARAM_IMPL_FOR(_mtx), PARAM_IMPL_FOR(_screenSize), PARAM_IMPL_FOR(_flipY));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  bool flipY{true};

  public:
  Project() {
    *_flipY = Var(true);
  }

  void warmup(SHContext *context) { 
    PARAM_WARMUP(context);
    flipY = _flipY.get().payload.boolValue;
  }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    using namespace linalg::aliases;

    float4x4 mat = toFloat4x4(_mtx.get());
    auto transformed = linalg::mul(mat, float4(toVec<float3>(input), 1.0f));

    float2 screenSize = toVec<float2>(_screenSize.get());
    float3 projected = (transformed.xyz() / transformed.w);

    if (flipY) {
      projected.y = -projected.y;
    }

    projected = (projected + float3(1.0f, 1.0f, 0.0f)) / float3(2.0f, 2.0f, 1.0f);
    projected *= float3(screenSize.x, screenSize.y, 0.0f);

    return Vec3(projected);
  }
};

struct Unproject {
  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a float3 vector representing the 3D vector where x and y are screen coordinates, and z is the depth value in screen space.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns a float3 vector representing the unprojected 3D point in world space.");
  }

  static SHOptionalString help() {
    return SHCCSTR(
        "This shard converts 2D screen coordinates back to 3D world coordinates using the inverse of a view-projection matrix. "
        "Both 3D and 2D coordinates are represented as float3 vectors (vectors with 3 float elements)."
        "It performs the reverse operation of the projection pipeline, including inverse matrix multiplication, "
        "and coordinate space transformations using the 4x4 view-projection matrix specified in the Matrix parameter and the "
        "screen size in the ScreenSize parameter.");
  }

  PARAM_PARAMVAR(_mtx, "Matrix", "The combined 4x4 view-projection matrix (sequence of four float4 vectors) to use.",
                 {CoreInfo::Float4x4Type, Type::VariableOf(CoreInfo::Float4x4Type)});
  PARAM_PARAMVAR(_screenSize, "ScreenSize", "The float2 vector representing the size of the screen or viewport in pixels.",
                 {CoreInfo::Float2Type, Type::VariableOf(CoreInfo::Float2Type)});
  PARAM_PARAMVAR(_depthRange, "DepthRange", "The float2 vector representing the range of depth values (near and far planes). Default is [0, 1].",
                 {CoreInfo::NoneType, CoreInfo::Float2Type, Type::VariableOf(CoreInfo::Float2Type)});
  PARAM_PARAMVAR(_flipY, "FlipY", "Flip Y coordinate (on by default)",
                 {CoreInfo::NoneType, CoreInfo::BoolType, Type::VariableOf(CoreInfo::BoolVarType)});
  PARAM_IMPL(PARAM_IMPL_FOR(_mtx), PARAM_IMPL_FOR(_screenSize), PARAM_IMPL_FOR(_depthRange), PARAM_IMPL_FOR(_flipY));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  bool flipY{true};

  public:
  Unproject() {
    *_flipY = Var(true);
  }

  void warmup(SHContext *context) { 
    PARAM_WARMUP(context);
    flipY = _flipY.get().payload.boolValue;
  }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    using namespace linalg::aliases;

    float2 depthRange(0.0f, 1.0f);
    if (!_depthRange.isNone()) {
      depthRange = (float2)toFloat2(_depthRange.get());
    }

    float a = -depthRange.y / (depthRange.y - depthRange.x);
    float b = -depthRange.x * depthRange.y / (depthRange.y - depthRange.x);

    float4 screenSpace = float4(toVec<float3>(input), 1.0f);
    float2 screenSize = toVec<float2>(_screenSize.get());

    float ndcDepth = (b / screenSpace.z) - a;

    float4 ndc =
        float4((screenSpace.x / screenSize.x) * 2.0f - 1.0f, (screenSpace.y / screenSize.y) * 2.0f - 1.0f, ndcDepth, 1.0f);

    if (flipY) {
      ndc.y = -ndc.y;
    }

    float4x4 mat = toFloat4x4(_mtx.get());
    float4x4 invMat = linalg::inverse(mat);
    auto transformed = linalg::mul(invMat, ndc);
    float3 unprojected = (transformed.xyz() / transformed.w);

    return Vec3(unprojected);
  }
};

struct QuaternionMultiply {
  Vec4 _output{};

  static SHTypesInfo inputTypes() { return CoreInfo::Float4Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a float4 vector representing the quaternion to be multiplied.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4Type; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns a float4 vector representing the resulting quaternion after multiplication.");
  }

  static SHOptionalString help() {
    return SHCCSTR("This shard multiplies two quaternions (represented as float4 vectors) together. It combines the two rotations by multiplying "
                   "the input quaternion with the operand quaternion. A float4 vector is a vector with 4 float elements.");
  }

  PARAM_PARAMVAR(_operand, "Operand", "The float4 vector representing the second quaternion to multiply the input quaternion with.",
                 {shards::CoreInfo::Float4Type, shards::CoreInfo::Float4VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_operand));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_operand.isNone()) {
      throw ActivationError("Expected an operand");
    }

    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output = linalg::qmul(toFloat4(input), toFloat4(_operand.get()));
    return _output;
  }
};

struct QuaternionSlerp {
  Vec4 _output{};

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a float value between 0 and 1 representing the interpolation factor.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4Type; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns a float4 vector representing the interpolated quaternion."); }

  static SHOptionalString help() {
    return SHCCSTR("This shard performs Spherical Linear Interpolation (SLERP) between two quaternions (represented as float4 vectors). "
                   "It smoothly interpolates between the quaternions specified in the 'First' parameter and 'Second' parameter "
                   "based on the input interpolation factor. A float4 vector is a vector with 4 float elements.");
  }

  PARAM_PARAMVAR(_a, "First", "The float4 vector representing the first quaternion to interpolate from.", {shards::CoreInfo::Float4Type, shards::CoreInfo::Float4VarType});
  PARAM_PARAMVAR(_b, "Second", "The float4 vector representing the second quaternion to interpolate to.", {shards::CoreInfo::Float4Type, shards::CoreInfo::Float4VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_a), PARAM_IMPL_FOR(_b));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_a.isNone()) {
      throw ActivationError("Expected a value");
    }

    if (_b.isNone()) {
      throw ActivationError("Expected a value");
    }

    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    float factor = input.payload.floatValue;
    auto &a = *reinterpret_cast<Vec4 *>(&_a.get());
    auto &b = *reinterpret_cast<Vec4 *>(&_b.get());
    _output = linalg::qslerp(*a, *b, factor);
    return _output;
  }
};

struct QuaternionRotate {
  Vec3 _output{};

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString inputHelp() { return SHCCSTR("Takes a float3 vector representing the 3D vector to be rotated."); }
  static SHTypesInfo outputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns a float3 vector representing the rotated 3D vector."); }

  static SHOptionalString help() {
    return SHCCSTR("This shard rotates the input 3D vector (represented as a float3) by the quaternion (represented as a float4) specified in the Operand parameter and "
                   "returns the resulting rotated 3D vector. A float4 vector is a vector with 4 float elements while a float3 "
                   "vector is a vector with 3 float elements.");
  }

  PARAM_PARAMVAR(_operand, "Operand", "The float4 vector representing the quaternion to rotate the input 3D vector by.",
                 {shards::CoreInfo::Float4Type, shards::CoreInfo::Float4VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_operand));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_operand.isNone()) {
      throw ActivationError("Expected an operand");
    }

    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output = linalg::qrot(toFloat4(_operand.get()), toFloat3(input));
    return _output;
  }
};

} // namespace LinAlg
} // namespace Math
} // namespace shards

SHARDS_REGISTER_FN(linalg) {
  using namespace shards::Math::LinAlg;
  REGISTER_SHARD("Math.Cross", Cross);
  REGISTER_SHARD("Math.Dot", Dot);
  REGISTER_SHARD("Math.Normalize", Normalize);
  REGISTER_SHARD("Math.LengthSquared", LengthSquared);
  REGISTER_SHARD("Math.Length", Length);
  REGISTER_SHARD("Math.MatMul", MatMul);
  REGISTER_SHARD("Math.Transpose", Transpose);
  REGISTER_SHARD("Math.Inverse", Inverse);
  REGISTER_SHARD("Math.Orthographic", Orthographic);
  REGISTER_SHARD("Math.Translation", Translation);
  REGISTER_SHARD("Math.Scaling", Scaling);
  REGISTER_SHARD("Math.Rotation", Rotation);
  REGISTER_SHARD("Math.LookAt", LookAt);
  REGISTER_SHARD("Math.AxisAngleX", AxisAngleX);
  REGISTER_SHARD("Math.AxisAngleY", AxisAngleY);
  REGISTER_SHARD("Math.AxisAngleZ", AxisAngleZ);
  REGISTER_SHARD("Math.DegreesToRadians", Deg2Rad);
  REGISTER_SHARD_ALIAS("DegreesToRadians", Deg2Rad);
  REGISTER_SHARD("Math.RadiansToDegrees", Rad2Deg);
  REGISTER_SHARD_ALIAS("RadiansToDegrees", Rad2Deg);
  REGISTER_SHARD("Math.MatIdentity", MatIdentity);
  REGISTER_SHARD("Math.Compose", Compose);
  REGISTER_SHARD("Math.Decompose", Decompose);
  REGISTER_SHARD("Math.Project", Project);
  REGISTER_SHARD("Math.Unproject", Unproject);
  REGISTER_SHARD("Math.QuatMultiply", QuaternionMultiply);
  REGISTER_SHARD("Math.QuatRotate", QuaternionRotate);
  REGISTER_SHARD("Math.Slerp", QuaternionSlerp);
}

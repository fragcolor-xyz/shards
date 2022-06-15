/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "linalg.hpp"

namespace shards {
namespace Math {
namespace LinAlg {

void Cross::Operation::operator()(SHVar &output, const SHVar &input, const SHVar &operand) {
  if (operand.valueType != Float3)
    throw ActivationError("LinAlg.Cross works only with Float3 types.");

  switch (input.valueType) {
  case Float3: {
    const SHInt3 mask1 = {1, 2, 0};
    const SHInt3 mask2 = {2, 0, 1};
    auto a1 = shufflevector(input.payload.float3Value, mask1);
    auto a2 = shufflevector(input.payload.float3Value, mask2);
    auto b1 = shufflevector(operand.payload.float3Value, mask1);
    auto b2 = shufflevector(operand.payload.float3Value, mask2);
    output.valueType = Float3;
    output.payload.float3Value = a1 * b2 - a2 * b1;
  } break;
  default:
    throw ActivationError("LinAlg.Cross works only with Float3 types.");
  }
}

SHVar Cross::activate(SHContext *context, const SHVar &input) {
  const Operation op;
  return doActivate(context, input, op);
}

void Dot::Operation::operator()(SHVar &output, const SHVar &input, const SHVar &operand) {
  if (operand.valueType != input.valueType)
    throw ActivationError("LinAlg.Dot works only with same input and operand types.");

  switch (input.valueType) {
  case Float2: {
    output.valueType = Float;
    output.payload.floatValue = input.payload.float2Value[0] * operand.payload.float2Value[0];
    output.payload.floatValue += input.payload.float2Value[1] * operand.payload.float2Value[1];
  } break;
  case Float3: {
    output.valueType = Float;
    output.payload.floatValue = input.payload.float3Value[0] * operand.payload.float3Value[0];
    output.payload.floatValue += input.payload.float3Value[1] * operand.payload.float3Value[1];
    output.payload.floatValue += input.payload.float3Value[2] * operand.payload.float3Value[2];
  } break;
  case Float4: {
    output.valueType = Float;
    output.payload.floatValue = input.payload.float4Value[0] * operand.payload.float4Value[0];
    output.payload.floatValue += input.payload.float4Value[1] * operand.payload.float4Value[1];
    output.payload.floatValue += input.payload.float4Value[2] * operand.payload.float4Value[2];
    output.payload.floatValue += input.payload.float4Value[3] * operand.payload.float4Value[3];
  } break;
  default:
    break;
  }
}

SHVar Dot::activate(SHContext *context, const SHVar &input) {
  const Operation op;
  return doActivate(context, input, op);
}

void Normalize::Operation::operator()(SHVar &output, const SHVar &input) {
  SHVar len{};
  lenOp(len, input);

  switch (input.valueType) {
  case Float2: {
    output.valueType = Float2;
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
  case Float3: {
    output.valueType = Float3;
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
  case Float4: {
    output.valueType = Float4;
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

SHVar Normalize::activate(SHContext *context, const SHVar &input) {
  const Operation op{_positiveOnly};
  return doActivate(context, input, op);
}

SHVar Normalize::activateFloatSeq(SHContext *context, const SHVar &input) {
  const auto len = input.payload.seqValue.len;
  _output.resize(len, SHVar{.valueType = SHType::Float});
  float vlen = 0.0;
  for (uint32_t i = 0; i < len; i++) {
    const auto f = input.payload.seqValue.elements[i].payload.floatValue;
    vlen += f * f;
  }
  vlen = __builtin_sqrt(vlen);
  if (vlen > 0 || !_positiveOnly) {
    // better branching here
    if (!_positiveOnly) {
      for (uint32_t i = 0; i < len; i++) {
        const auto f = input.payload.seqValue.elements[i].payload.floatValue;
        _output[i].payload.floatValue = f / vlen;
      }
    } else {
      for (uint32_t i = 0; i < len; i++) {
        const auto f = input.payload.seqValue.elements[i].payload.floatValue;
        _output[i].payload.floatValue = ((f / vlen) + 1.0) / 2.0;
      }
    }
    return Var(_output);
  } else {
    return input;
  }
}

SHVar MatMul::activate(SHContext *context, const SHVar &input) {
  auto &operand = _operand.get();
  // expect SeqSeq as in 2x 2D arrays or Seq1 Mat @ Vec
  if (_opType == SeqSeq) {
#define MATMUL_OP(_v1_, _v2_, _n_)                                                                             \
  shards::arrayResize(_result.payload.seqValue, _n_);                                                     \
  linalg::aliases::_v1_ *a = reinterpret_cast<linalg::aliases::_v1_ *>(&input.payload.seqValue.elements[0]);   \
  linalg::aliases::_v1_ *b = reinterpret_cast<linalg::aliases::_v1_ *>(&operand.payload.seqValue.elements[0]); \
  linalg::aliases::_v1_ *c = reinterpret_cast<linalg::aliases::_v1_ *>(&_result.payload.seqValue.elements[0]); \
  *c = linalg::mul(*a, *b);                                                                                    \
  for (auto i = 0; i < _n_; i++) {                                                                             \
    _result.payload.seqValue.elements[i].valueType = _v2_;                                                     \
  }
    _result.valueType = Seq;
    switch (input.payload.seqValue.elements[0].valueType) {
    case Float2: {
      MATMUL_OP(double2x2, Float2, 2);
    } break;
    case Float3: {
      MATMUL_OP(float3x3, Float3, 3);
    } break;
    case Float4: {
      MATMUL_OP(float4x4, Float4, 4);
    } break;
    default:
      throw ActivationError("Invalid value type for MatMul");
    }
#undef MATMUL_OP
    return _result;
  } else if (_opType == Seq1) {
#define MATMUL_OP(_v1_, _v2_, _n_, _v3_, _v3v_)                                                              \
  linalg::aliases::_v1_ *a = reinterpret_cast<linalg::aliases::_v1_ *>(&input.payload.seqValue.elements[0]); \
  linalg::aliases::_v3_ *b = reinterpret_cast<linalg::aliases::_v3_ *>(&operand.payload._v3v_);              \
  linalg::aliases::_v3_ *c = reinterpret_cast<linalg::aliases::_v3_ *>(&_result.payload._v3v_);              \
  *c = linalg::mul(*a, *b);                                                                                  \
  _result.valueType = _v2_;
    switch (input.payload.seqValue.elements[0].valueType) {
    case Float2: {
      MATMUL_OP(double2x2, Float2, 2, double2, float2Value);
    } break;
    case Float3: {
      MATMUL_OP(float3x3, Float3, 3, float3, float3Value);
    } break;
    case Float4: {
      MATMUL_OP(float4x4, Float4, 4, float4, float4Value);
    } break;
    default:
      throw ActivationError("Invalid value type for MatMul");
    }
    return _result;
  } else {
    throw ActivationError("MatMul expects either Mat (Seq of FloatX) @ Mat or "
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
  case Float2:
    width = 2;
    break;
  case Float3:
    width = 3;
    break;
  case Float4:
    width = 4;
    break;
  default:
    break;
  }

  _result.valueType = Seq;
  shards::arrayResize(_result.payload.seqValue, width);

  double v1{}, v2{}, v3{}, v4{};
  for (size_t w = 0; w < width; w++) {
    switch (height) {
    case 2:
      _result.payload.seqValue.elements[w].valueType = Float2;
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
      _result.payload.seqValue.elements[w].valueType = Float3;
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
      _result.payload.seqValue.elements[w].valueType = Float4;
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

void registerShards() {
  REGISTER_SHARD("Math.Cross", Cross);
  REGISTER_SHARD("Math.Dot", Dot);
  REGISTER_SHARD("Math.Normalize", Normalize);
  REGISTER_SHARD("Math.LengthSquared", LengthSquared);
  REGISTER_SHARD("Math.Length", Length);
  REGISTER_SHARD("Math.MatMul", MatMul);
  REGISTER_SHARD("Math.Transpose", Transpose);
  REGISTER_SHARD("Math.Orthographic", Orthographic);
  REGISTER_SHARD("Math.Translation", Translation);
  REGISTER_SHARD("Math.Scaling", Scaling);
  REGISTER_SHARD("Math.Rotation", Rotation);
  REGISTER_SHARD("Math.LookAt", LookAt);
  REGISTER_SHARD("Math.AxisAngleX", AxisAngleX);
  REGISTER_SHARD("Math.AxisAngleY", AxisAngleY);
  REGISTER_SHARD("Math.AxisAngleZ", AxisAngleZ);
  REGISTER_SHARD("Math.DegreesToRadians", Deg2Rad);
  REGISTER_SHARD("Math.RadiansToDegrees", Rad2Deg);
}

}; // namespace LinAlg
}; // namespace Math
}; // namespace shards

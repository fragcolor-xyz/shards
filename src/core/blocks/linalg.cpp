/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#include "math.h"
#include "shared.hpp"
#include <linalg_shim.hpp>

namespace chainblocks {
namespace Math {
namespace LinAlg {
struct VectorUnaryBase : public UnaryBase {
  static CBTypesInfo inputTypes() { return CoreInfo::FloatVectors; }

  static CBTypesInfo outputTypes() { return CoreInfo::FloatVectors; }

  CBTypeInfo compose(const CBInstanceData &data) { return data.inputType; }

  template <class Operation>
  CBVar doActivate(CBContext *context, const CBVar &input, Operation operate) {
    if (input.valueType == Seq) {
      _result.valueType = Seq;
      chainblocks::arrayResize(_result.payload.seqValue, 0);
      for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
        CBVar scratch;
        operate(scratch, input);
        chainblocks::arrayPush(_result.payload.seqValue, scratch);
      }
      return _result;
    } else {
      CBVar scratch;
      operate(scratch, input);
      return scratch;
    }
  }
};

struct VectorBinaryBase : public BinaryBase {
  static CBTypesInfo inputTypes() { return CoreInfo::FloatVectors; }

  static CBTypesInfo outputTypes() { return CoreInfo::FloatVectors; }

  static inline ParamsInfo paramsInfo = ParamsInfo(ParamsInfo::Param(
      "Operand", CBCCSTR("The operand."), CoreInfo::FloatVectorsOrVar));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  template <class Operation>
  CBVar doActivate(CBContext *context, const CBVar &input, Operation operate) {
    auto &operand = _operand.get();
    if (_opType == Normal) {
      CBVar scratch;
      operate(scratch, input, operand);
      return scratch;
    } else if (_opType == Seq1) {
      _result.valueType = Seq;
      chainblocks::arrayResize(_result.payload.seqValue, 0);
      for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
        CBVar scratch;
        operate(scratch, input.payload.seqValue.elements[i], operand);
        chainblocks::arrayPush(_result.payload.seqValue, scratch);
      }
      return _result;
    } else {
      _result.valueType = Seq;
      chainblocks::arrayResize(_result.payload.seqValue, 0);
      for (uint32_t i = 0;
           i < input.payload.seqValue.len && i < operand.payload.seqValue.len;
           i++) {
        CBVar scratch;
        operate(scratch, input.payload.seqValue.elements[i],
                operand.payload.seqValue.elements[i]);
        chainblocks::arrayPush(_result.payload.seqValue, scratch);
      }
      return _result;
    }
  }
};

struct Cross : public VectorBinaryBase {
  struct Operation {
    void operator()(CBVar &output, const CBVar &input, const CBVar &operand) {
      if (operand.valueType != Float3)
        throw ActivationError("LinAlg.Cross works only with Float3 types.");

      switch (input.valueType) {
      case Float3: {
        const CBInt3 mask1 = {1, 2, 0};
        const CBInt3 mask2 = {2, 0, 1};
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
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct Dot : public VectorBinaryBase {
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    VectorBinaryBase::compose(data);
    return CoreInfo::FloatType;
  }

  struct Operation {
    void operator()(CBVar &output, const CBVar &input, const CBVar &operand) {
      if (operand.valueType != input.valueType)
        throw ActivationError(
            "LinAlg.Dot works only with same input and operand types.");

      switch (input.valueType) {
      case Float2: {
        output.valueType = Float;
        output.payload.floatValue =
            input.payload.float2Value[0] * operand.payload.float2Value[0];
        output.payload.floatValue +=
            input.payload.float2Value[1] * operand.payload.float2Value[1];
      } break;
      case Float3: {
        output.valueType = Float;
        output.payload.floatValue =
            input.payload.float3Value[0] * operand.payload.float3Value[0];
        output.payload.floatValue +=
            input.payload.float3Value[1] * operand.payload.float3Value[1];
        output.payload.floatValue +=
            input.payload.float3Value[2] * operand.payload.float3Value[2];
      } break;
      case Float4: {
        output.valueType = Float;
        output.payload.floatValue =
            input.payload.float4Value[0] * operand.payload.float4Value[0];
        output.payload.floatValue +=
            input.payload.float4Value[1] * operand.payload.float4Value[1];
        output.payload.floatValue +=
            input.payload.float4Value[2] * operand.payload.float4Value[2];
        output.payload.floatValue +=
            input.payload.float4Value[3] * operand.payload.float4Value[3];
      } break;
      default:
        break;
      }
    };
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct LengthSquared : public VectorUnaryBase {
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    VectorUnaryBase::compose(data);
    return CoreInfo::FloatType;
  }

  struct Operation {
    Dot::Operation dotOp;
    void operator()(CBVar &output, const CBVar &input) {
      dotOp(output, input, input);
    }
  };
  CBVar activate(CBContext *context, const CBVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct Length : public VectorUnaryBase {
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    VectorUnaryBase::compose(data);
    return CoreInfo::FloatType;
  }

  struct Operation {
    LengthSquared::Operation lenOp;
    void operator()(CBVar &output, const CBVar &input) {
      lenOp(output, input);
      output.payload.floatValue = __builtin_sqrt(output.payload.floatValue);
    }
  };
  CBVar activate(CBContext *context, const CBVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct Normalize : public VectorUnaryBase {
  struct Operation {
    Length::Operation lenOp;
    void operator()(CBVar &output, const CBVar &input) {
      CBVar len{};
      lenOp(len, input);

      switch (input.valueType) {
      case Float2: {
        output.valueType = Float2;
        if (len.payload.floatValue > 0) {
          CBFloat2 vlen = {len.payload.floatValue, len.payload.floatValue};
          output.payload.float2Value = input.payload.float2Value / vlen;
        } else {
          output.payload.float2Value = input.payload.float2Value;
        }
      } break;
      case Float3: {
        output.valueType = Float3;
        if (len.payload.floatValue > 0) {
          CBFloat3 vlen = {float(len.payload.floatValue),
                           float(len.payload.floatValue),
                           float(len.payload.floatValue)};
          output.payload.float3Value = input.payload.float3Value / vlen;
        } else {
          output.payload.float3Value = input.payload.float3Value;
        }
      } break;
      case Float4: {
        output.valueType = Float4;
        if (len.payload.floatValue > 0) {
          CBFloat4 vlen = {
              float(len.payload.floatValue), float(len.payload.floatValue),
              float(len.payload.floatValue), float(len.payload.floatValue)};
          output.payload.float4Value = input.payload.float4Value / vlen;
        } else {
          output.payload.float4Value = input.payload.float4Value;
        }
      } break;
      default:
        break;
      }
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct MatMul : public VectorBinaryBase {
  // MatMul is special...
  // Mat @ Mat = Mat
  // Mat @ Vec = Vec
  // If ever becomes a bottle neck, valgrind and optimize

  CBTypeInfo compose(const CBInstanceData &data) {
    BinaryBase::compose(data);
    if (_opType == SeqSeq) {
      return data.inputType;
    } else {
      if (data.inputType.seqTypes.len != 1) {
        throw CBException("MatMul expected a unique Seq inner type.");
      }
      return data.inputType.seqTypes.elements[0];
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto &operand = _operand.get();
    // expect SeqSeq as in 2x 2D arrays or Seq1 Mat @ Vec
    if (_opType == SeqSeq) {
#define MATMUL_OP(_v1_, _v2_, _n_)                                             \
  chainblocks::arrayResize(_result.payload.seqValue, _n_);                     \
  linalg::aliases::_v1_ *a = reinterpret_cast<linalg::aliases::_v1_ *>(        \
      &input.payload.seqValue.elements[0]);                                    \
  linalg::aliases::_v1_ *b = reinterpret_cast<linalg::aliases::_v1_ *>(        \
      &operand.payload.seqValue.elements[0]);                                  \
  linalg::aliases::_v1_ *c = reinterpret_cast<linalg::aliases::_v1_ *>(        \
      &_result.payload.seqValue.elements[0]);                                  \
  *c = linalg::mul(*a, *b);                                                    \
  for (auto i = 0; i < _n_; i++) {                                             \
    _result.payload.seqValue.elements[i].valueType = _v2_;                     \
  }
      _result.valueType = Seq;
      switch (input.payload.seqValue.elements[0].valueType) {
      case Float2: {
        MATMUL_OP(float2x2, Float2, 2);
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
#define MATMUL_OP(_v1_, _v2_, _n_, _v3_, _v3v_)                                \
  linalg::aliases::_v1_ *a = reinterpret_cast<linalg::aliases::_v1_ *>(        \
      &input.payload.seqValue.elements[0]);                                    \
  linalg::aliases::_v3_ *b =                                                   \
      reinterpret_cast<linalg::aliases::_v3_ *>(&operand.payload._v3v_);       \
  linalg::aliases::_v3_ *c =                                                   \
      reinterpret_cast<linalg::aliases::_v3_ *>(&_result.payload._v3v_);       \
  *c = linalg::mul(*a, *b);                                                    \
  _result.valueType = _v2_;
      switch (input.payload.seqValue.elements[0].valueType) {
      case Float2: {
        MATMUL_OP(float2x2, Float2, 2, float2, float2Value);
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
      throw ActivationError(
          "MatMul expects either Mat (Seq of FloatX) @ Mat or "
          "Mat @ Vec (FloatX)");
    }
  }
};

struct Transpose : public VectorUnaryBase {
  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.inputType.basicType != Seq) {
      throw ComposeError("Transpose expected a Seq matrix array as input.");
    }
    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
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
    chainblocks::arrayResize(_result.payload.seqValue, width);

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
};

struct Orthographic : VectorUnaryBase {
  double _width = 1280;
  double _height = 720;
  double _near = 0.0;
  double _far = 1000.0;

  void setup() {
    _result.valueType = Seq;
    chainblocks::arrayResize(_result.payload.seqValue, 4);
    for (auto i = 0; i < 4; i++) {
      _result.payload.seqValue.elements[i] = Var::Empty;
      _result.payload.seqValue.elements[i].valueType = Float4;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    return CoreInfo::Float4SeqType;
  }

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float4SeqType; }

  // left, right, bottom, top, near, far
  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Width", CBCCSTR("Width size."), CoreInfo::IntOrFloat),
      ParamsInfo::Param("Height", CBCCSTR("Height size."),
                        CoreInfo::IntOrFloat),
      ParamsInfo::Param("Near", CBCCSTR("Near plane."), CoreInfo::IntOrFloat),
      ParamsInfo::Param("Far", CBCCSTR("Far plane."), CoreInfo::IntOrFloat));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _width = double(Var(value));
      break;
    case 1:
      _height = double(Var(value));
      break;
    case 2:
      _near = double(Var(value));
      break;
    case 3:
      _far = double(Var(value));
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_width);
    case 1:
      return Var(_height);
    case 2:
      return Var(_near);
    case 3:
      return Var(_far);
    default:
      break;
    }
    return Var();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto right = 0.5 * _width;
    auto left = -right;
    auto top = 0.5 * _height;
    auto bottom = -top;
    auto zrange = 1.0 / (_far - _near);
    _result.payload.seqValue.elements[0].payload.float4Value[0] =
        2.0 / (right - left);
    _result.payload.seqValue.elements[1].payload.float4Value[1] =
        2.0 / (top - bottom);
    _result.payload.seqValue.elements[2].payload.float4Value[2] = -zrange;
    _result.payload.seqValue.elements[3].payload.float4Value[0] =
        (left + right) / (left - right);
    _result.payload.seqValue.elements[3].payload.float4Value[1] =
        (top + bottom) / (bottom - top);
    _result.payload.seqValue.elements[3].payload.float4Value[2] =
        -_near * zrange;
    _result.payload.seqValue.elements[3].payload.float4Value[3] = 1.0;
    return _result;
  }
};

struct Translation {
  Mat4 _output{};

  static CBTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto v3 = reinterpret_cast<const Vec3 *>(&input);
    _output = linalg::translation_matrix(*v3);
    return _output;
  }
};

struct Scaling {
  Mat4 _output{};

  static CBTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto v3 = reinterpret_cast<const Vec3 *>(&input);
    _output = linalg::scaling_matrix(*v3);
    return _output;
  }
};

struct Rotation {
  Mat4 _output{};

  static CBTypesInfo inputTypes() { return CoreInfo::Float4Type; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  CBVar activate(CBContext *context, const CBVar &input) {
    // quaternion
    auto v4 = reinterpret_cast<const Vec4 *>(&input);
    _output = linalg::rotation_matrix(*v4);
    return _output;
  }
};

template <const linalg::aliases::float3 &AXIS> struct AxisAngle {
  Vec4 _output{};

  static CBTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float4Type; }

  CBVar activate(CBContext *context, const CBVar &input) {
    _output = linalg::rotation_quat(AXIS, float(input.payload.floatValue));
    return _output;
  }
};

constexpr linalg::vec<float, 3> AxisX{1.0, 0.0, 0.0};
constexpr linalg::vec<float, 3> AxisY{0.0, 1.0, 0.0};
constexpr linalg::vec<float, 3> AxisZ{0.0, 0.0, 1.0};

using AxisAngleX = AxisAngle<AxisX>;
using AxisAngleY = AxisAngle<AxisY>;
using AxisAngleZ = AxisAngle<AxisZ>;

struct Deg2Rad {
  const double PI = 3.141592653589793238463;

  static CBTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    return Var(input.payload.floatValue * (PI / 180.0));
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("Math.Cross", Cross);
  REGISTER_CBLOCK("Math.Dot", Dot);
  REGISTER_CBLOCK("Math.Normalize", Normalize);
  REGISTER_CBLOCK("Math.LengthSquared", LengthSquared);
  REGISTER_CBLOCK("Math.Length", Length);
  REGISTER_CBLOCK("Math.MatMul", MatMul);
  REGISTER_CBLOCK("Math.Transpose", Transpose);
  REGISTER_CBLOCK("Math.Orthographic", Orthographic);
  REGISTER_CBLOCK("Math.Translation", Translation);
  REGISTER_CBLOCK("Math.Scaling", Scaling);
  REGISTER_CBLOCK("Math.Rotation", Rotation);
  REGISTER_CBLOCK("Math.AxisAngleX", AxisAngleX);
  REGISTER_CBLOCK("Math.AxisAngleY", AxisAngleY);
  REGISTER_CBLOCK("Math.AxisAngleZ", AxisAngleZ);
  REGISTER_CBLOCK("Math.DegreesToRadians", Deg2Rad);
}
}; // namespace LinAlg
}; // namespace Math
}; // namespace chainblocks

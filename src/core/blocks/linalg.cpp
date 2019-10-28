/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "math.h"
#include "shared.hpp"

namespace chainblocks {
namespace Math {
namespace LinAlg {
struct VectorUnaryBase : public UnaryBase {
  static CBTypesInfo inputTypes() {
    return CBTypesInfo(SharedTypes::vectorsInfo);
  }

  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::vectorsInfo);
  }

  template <class Operation>
  ALWAYS_INLINE CBVar doActivate(CBContext *context, const CBVar &input,
                                 Operation operate) {
    CBVar output{};
    if (input.valueType == Seq) {
      stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);
      for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {
        operate(output, input);
        stbds_arrpush(_cachedSeq.payload.seqValue, output);
      }
      return _cachedSeq;
    } else {
      operate(output, input);
      return output;
    }
  }
};

struct VectorBinaryBase : public BinaryBase {
  static CBTypesInfo inputTypes() {
    return CBTypesInfo(SharedTypes::vectorsInfo);
  }

  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::vectorsInfo);
  }

  static inline ParamsInfo paramsInfo = ParamsInfo(ParamsInfo::Param(
      "Operand", "The operand.", CBTypesInfo(SharedTypes::vectorsCtxInfo)));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  template <class Operation>
  ALWAYS_INLINE CBVar doActivate(CBContext *context, const CBVar &input,
                                 Operation operate) {
    if (_operand.valueType == ContextVar && _ctxOperand == nullptr) {
      _ctxOperand = contextVariable(context, _operand.payload.stringValue);
    }
    auto &operand = _ctxOperand ? *_ctxOperand : _operand;
    CBVar output{};
    if (_opType == Normal) {
      operate(output, input, operand);
      return output;
    } else if (_opType == Seq1) {
      stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);
      for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {
        operate(output, input.payload.seqValue[i], operand);
        stbds_arrpush(_cachedSeq.payload.seqValue, output);
      }
      return _cachedSeq;
    } else {
      stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);
      for (auto i = 0; i < stbds_arrlen(input.payload.seqValue) &&
                       i < stbds_arrlen(operand.payload.seqValue);
           i++) {
        operate(output, input.payload.seqValue[i], operand.payload.seqValue[i]);
        stbds_arrpush(_cachedSeq.payload.seqValue, output);
      }
      return _cachedSeq;
    }
  }
};

struct Cross : public VectorBinaryBase {
  struct Operation {
    void operator()(CBVar &output, const CBVar &input, const CBVar &operand) {
      if (operand.valueType != Float3)
        throw CBException("LinAlg.Cross works only with Float3 types.");

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
        throw CBException("LinAlg.Cross works only with Float3 types.");
      }
    }
  };

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct Dot : public VectorBinaryBase {
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::floatsInfo);
  }

  struct Operation {
    void operator()(CBVar &output, const CBVar &input, const CBVar &operand) {
      if (operand.valueType != input.valueType)
        throw CBException(
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

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct LengthSquared : public VectorUnaryBase {
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::floatsInfo);
  }

  struct Operation {
    Dot::Operation dotOp;
    void operator()(CBVar &output, const CBVar &input) {
      dotOp(output, input, input);
    }
  };
  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct Length : public VectorUnaryBase {
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::floatsInfo);
  }

  struct Operation {
    LengthSquared::Operation lenOp;
    void operator()(CBVar &output, const CBVar &input) {
      lenOp(output, input);
      output.payload.floatValue = __builtin_sqrt(output.payload.floatValue);
    }
  };
  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
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

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct MatMul : public VectorBinaryBase {
  // MatMul is special...
  // Mat @ Mat = Mat
  // Mat @ Vec = Vec
  // If ever becomes a bottle neck, valgrind and optimize

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    BinaryBase::inferTypes(inputType, consumableVariables);
    if (_opType == SeqSeq) {
      return inputType;
    } else {
      return *inputType.seqType;
    }
  }

  CBVar mvmul(const CBVar &a, const CBVar &b) {
    CBVar output;
    output.valueType = b.valueType;
    auto dims = stbds_arrlen(a.payload.seqValue);
    for (auto i = 0; i < dims; i++) {
      const auto &x = a.payload.seqValue[i];
      if (x.valueType != b.valueType) {
        // tbh this should be supported tho...
        throw CBException("MatMul expected same Float vector types");
      }

      switch (x.valueType) {
      case Float2: {
        output.payload.float2Value[i] =
            x.payload.float2Value[0] * b.payload.float2Value[0];
        output.payload.float2Value[i] +=
            x.payload.float2Value[1] * b.payload.float2Value[1];
      } break;
      case Float3: {
        output.payload.float3Value[i] =
            x.payload.float3Value[0] * b.payload.float3Value[0];
        output.payload.float3Value[i] +=
            x.payload.float3Value[1] * b.payload.float3Value[1];
        output.payload.float3Value[i] +=
            x.payload.float3Value[2] * b.payload.float3Value[2];
      } break;
      case Float4: {
        output.payload.float4Value[i] =
            x.payload.float4Value[0] * b.payload.float4Value[0];
        output.payload.float4Value[i] +=
            x.payload.float4Value[1] * b.payload.float4Value[1];
        output.payload.float4Value[i] +=
            x.payload.float4Value[2] * b.payload.float4Value[2];
        output.payload.float4Value[i] +=
            x.payload.float4Value[3] * b.payload.float4Value[3];
      } break;
      default:
        break;
      }
    }
    return output;
  }

  void mmmul(const CBVar &a, const CBVar &b) {
    auto dima = stbds_arrlen(a.payload.seqValue);
    auto dimb = stbds_arrlen(a.payload.seqValue);
    if (dima != dimb) {
      throw CBException(
          "MatMul expected 2 arrays with the same number of columns");
    }

    stbds_arrsetlen(_cachedSeq.payload.seqValue, dima);
    for (auto i = 0; i < dima; i++) {
      const auto &y = b.payload.seqValue[i];
      const auto r = mvmul(a, y);
      _cachedSeq.payload.seqValue[i] = r;
    }
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    if (_operand.valueType == ContextVar && _ctxOperand == nullptr) {
      _ctxOperand = contextVariable(context, _operand.payload.stringValue);
    }
    const auto &operand = _ctxOperand ? *_ctxOperand : _operand;
    // expect SeqSeq as in 2x 2D arrays or Seq1 Mat @ Vec
    if (_opType == SeqSeq) {
      mmmul(input, operand);
      return _cachedSeq;
    } else if (_opType == Seq1) {
      return mvmul(input, operand);
    } else {
      throw CBException("MatMul expects either Mat (Seq of FloatX) @ Mat or "
                        "Mat @ Vec (FloatX)");
    }
  }
};

struct Transpose : public VectorUnaryBase {
  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    if (inputType.basicType != Seq) {
      throw CBException("Transpose expected a Seq matrix array as input.");
    }
    return inputType;
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto height = stbds_arrlen(input.payload.seqValue);
    if (height < 2 || height > 4) {
      // todo 2x1 should be go too
      throw CBException("Transpose expects a 2x2 to 4x4 matrix array.");
    }

    decltype(height) width = 0;
    switch (input.payload.seqValue[0].valueType) {
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

    stbds_arrsetlen(_cachedSeq.payload.seqValue, width);

    double v1, v2, v3, v4;
    for (auto w = 0; w < width; w++) {
      switch (height) {
      case 2:
        _cachedSeq.payload.seqValue[w].valueType = Float2;
        switch (width) {
        case 2:
          v1 = input.payload.seqValue[0].payload.float2Value[w];
          v2 = input.payload.seqValue[1].payload.float2Value[w];
          break;
        case 3:
          v1 = input.payload.seqValue[0].payload.float3Value[w];
          v2 = input.payload.seqValue[1].payload.float3Value[w];
          break;
        case 4:
          v1 = input.payload.seqValue[0].payload.float4Value[w];
          v2 = input.payload.seqValue[1].payload.float4Value[w];
          break;
        default:
          break;
        }
        _cachedSeq.payload.seqValue[w].payload.float2Value[0] = v1;
        _cachedSeq.payload.seqValue[w].payload.float2Value[1] = v2;
        break;
      case 3:
        _cachedSeq.payload.seqValue[w].valueType = Float3;
        switch (width) {
        case 2:
          v1 = input.payload.seqValue[0].payload.float2Value[w];
          v2 = input.payload.seqValue[1].payload.float2Value[w];
          v3 = input.payload.seqValue[2].payload.float2Value[w];
          break;
        case 3:
          v1 = input.payload.seqValue[0].payload.float3Value[w];
          v2 = input.payload.seqValue[1].payload.float3Value[w];
          v3 = input.payload.seqValue[2].payload.float3Value[w];
          break;
        case 4:
          v1 = input.payload.seqValue[0].payload.float4Value[w];
          v2 = input.payload.seqValue[1].payload.float4Value[w];
          v3 = input.payload.seqValue[2].payload.float4Value[w];
          break;
        default:
          break;
        }
        _cachedSeq.payload.seqValue[w].payload.float3Value[0] = v1;
        _cachedSeq.payload.seqValue[w].payload.float3Value[1] = v2;
        _cachedSeq.payload.seqValue[w].payload.float3Value[2] = v3;
        break;
      case 4:
        _cachedSeq.payload.seqValue[w].valueType = Float4;
        switch (width) {
        case 2:
          v1 = input.payload.seqValue[0].payload.float2Value[w];
          v2 = input.payload.seqValue[1].payload.float2Value[w];
          v3 = input.payload.seqValue[2].payload.float2Value[w];
          v4 = input.payload.seqValue[3].payload.float2Value[w];
          break;
        case 3:
          v1 = input.payload.seqValue[0].payload.float3Value[w];
          v2 = input.payload.seqValue[1].payload.float3Value[w];
          v3 = input.payload.seqValue[2].payload.float3Value[w];
          v4 = input.payload.seqValue[3].payload.float3Value[w];
          break;
        case 4:
          v1 = input.payload.seqValue[0].payload.float4Value[w];
          v2 = input.payload.seqValue[1].payload.float4Value[w];
          v3 = input.payload.seqValue[2].payload.float4Value[w];
          v4 = input.payload.seqValue[3].payload.float4Value[w];
          break;
        default:
          break;
        }
        _cachedSeq.payload.seqValue[w].payload.float4Value[0] = v1;
        _cachedSeq.payload.seqValue[w].payload.float4Value[1] = v2;
        _cachedSeq.payload.seqValue[w].payload.float4Value[2] = v3;
        _cachedSeq.payload.seqValue[w].payload.float4Value[3] = v4;
        break;
      }
    }
    return _cachedSeq;
  }
};

#define LINALG_BINARY_BLOCK(_name_)                                            \
  RUNTIME_BLOCK(Math.LinAlg, _name_);                                          \
  RUNTIME_BLOCK_destroy(_name_);                                               \
  RUNTIME_BLOCK_cleanup(_name_);                                               \
  RUNTIME_BLOCK_setup(_name_);                                                 \
  RUNTIME_BLOCK_inputTypes(_name_);                                            \
  RUNTIME_BLOCK_outputTypes(_name_);                                           \
  RUNTIME_BLOCK_parameters(_name_);                                            \
  RUNTIME_BLOCK_inferTypes(_name_);                                            \
  RUNTIME_BLOCK_consumedVariables(_name_);                                     \
  RUNTIME_BLOCK_setParam(_name_);                                              \
  RUNTIME_BLOCK_getParam(_name_);                                              \
  RUNTIME_BLOCK_activate(_name_);                                              \
  RUNTIME_BLOCK_END(_name_);

LINALG_BINARY_BLOCK(Cross);
LINALG_BINARY_BLOCK(Dot);
LINALG_BINARY_BLOCK(MatMul);

#define LINALG_UNARY_BLOCK(_name_)                                             \
  RUNTIME_BLOCK(Math.LinAlg, _name_);                                          \
  RUNTIME_BLOCK_destroy(_name_);                                               \
  RUNTIME_BLOCK_setup(_name_);                                                 \
  RUNTIME_BLOCK_inputTypes(_name_);                                            \
  RUNTIME_BLOCK_outputTypes(_name_);                                           \
  RUNTIME_BLOCK_activate(_name_);                                              \
  RUNTIME_BLOCK_END(_name_);

LINALG_UNARY_BLOCK(Normalize);
LINALG_UNARY_BLOCK(LengthSquared);
LINALG_UNARY_BLOCK(Length);
LINALG_UNARY_BLOCK(Transpose);

void registerBlocks() {
  chainblocks::registerBlock("Math.LinAlg.Cross", createBlockCross);
  chainblocks::registerBlock("Math.LinAlg.Dot", createBlockDot);
  chainblocks::registerBlock("Math.LinAlg.Normalize", createBlockNormalize);
  chainblocks::registerBlock("Math.LinAlg.LengthSquared",
                             createBlockLengthSquared);
  chainblocks::registerBlock("Math.LinAlg.Length", createBlockLength);
  chainblocks::registerBlock("Math.LinAlg.MatMul", createBlockMatMul);
  chainblocks::registerBlock("Math.LinAlg.Transpose", createBlockTranspose);
}
}; // namespace LinAlg
}; // namespace Math
}; // namespace chainblocks

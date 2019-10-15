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

void registerBlocks() {
  chainblocks::registerBlock("Math.LinAlg.Cross", createBlockCross);
  chainblocks::registerBlock("Math.LinAlg.Dot", createBlockDot);
  chainblocks::registerBlock("Math.LinAlg.Normalize", createBlockNormalize);
  chainblocks::registerBlock("Math.LinAlg.LengthSquared",
                             createBlockLengthSquared);
  chainblocks::registerBlock("Math.LinAlg.Length", createBlockLength);
}
}; // namespace LinAlg
}; // namespace Math
}; // namespace chainblocks

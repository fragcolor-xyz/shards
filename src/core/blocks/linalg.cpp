#include "math.h"
#include "shared.hpp"

namespace chainblocks {
namespace Math {
namespace LinAlg {
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
      for (auto i = 0; i < stbds_arrlen(input.payload.seqValue),
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
  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    struct {
      void operator()(CBVar &output, const CBVar &input, const CBVar &operand) {
        switch (input.valueType) {
        case Float3: {
          const CBInt3 mask1 = {1, 2, 0};
          const CBInt3 mask2 = {2, 0, 1};
          auto a1 = __builtin_shuffle(input.payload.float3Value, mask1);
          auto a2 = __builtin_shuffle(input.payload.float3Value, mask2);
          auto b1 = __builtin_shuffle(operand.payload.float3Value, mask1);
          auto b2 = __builtin_shuffle(operand.payload.float3Value, mask2);
          output.valueType = Float3;
          output.payload.float3Value = a1 * b2 - a2 * b1;
        } break;
        default:
          throw CBException("LinAlg.Cross works only with Float3 types.");
        }
      }
    } crossOp;
    return doActivate(context, input, crossOp);
  }
};

RUNTIME_BLOCK(Math.LinAlg, Cross);
RUNTIME_BLOCK_destroy(Cross);
RUNTIME_BLOCK_cleanup(Cross);
RUNTIME_BLOCK_setup(Cross);
RUNTIME_BLOCK_inputTypes(Cross);
RUNTIME_BLOCK_outputTypes(Cross);
RUNTIME_BLOCK_parameters(Cross);
RUNTIME_BLOCK_inferTypes(Cross);
RUNTIME_BLOCK_consumedVariables(Cross);
RUNTIME_BLOCK_setParam(Cross);
RUNTIME_BLOCK_getParam(Cross);
RUNTIME_BLOCK_activate(Cross);
RUNTIME_BLOCK_END(Cross);

void registerBlocks() {
  chainblocks::registerBlock("Math.LinAlg.Cross", createBlockCross);
}
}; // namespace LinAlg
}; // namespace Math
}; // namespace chainblocks

/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "shared.hpp"

namespace chainblocks {
static ParamsInfo condParamsInfo = ParamsInfo(
    ParamsInfo::Param(
        "Chains",
        "A sequence of chains, interleaving condition test predicate and "
        "action to execute if the condition matches.",
        CBTypesInfo(SharedTypes::blockSeqsOrNoneInfo)),
    ParamsInfo::Param(
        "Passthrough",
        "The input of this block will be the output. (default: true)",
        CBTypesInfo(SharedTypes::boolInfo)),
    ParamsInfo::Param("Threading",
                      "Will not short circuit after the first true test "
                      "expression. The threaded value gets used in only the "
                      "action and not the test part of the clause.",
                      CBTypesInfo(SharedTypes::boolInfo)));

struct Cond {
  CBVar _chains{};
  std::vector<std::vector<CBlock *>> _conditions;
  std::vector<std::vector<CBlock *>> _actions;
  bool _passthrough = true;
  bool _threading = false;
  CBValidationResult _chainValidation{};

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBParametersInfo parameters() {
    return CBParametersInfo(condParamsInfo);
  }

  void cleanup() {
    for (const auto &blocks : _conditions) {
      for (auto block : blocks) {
        block->cleanup(block);
      }
    }
    for (const auto &blocks : _actions) {
      for (auto block : blocks) {
        block->cleanup(block);
      }
    }
  }

  void destroy() {
    for (const auto &blocks : _conditions) {
      for (auto block : blocks) {
        block->cleanup(block);
        block->destroy(block);
      }
    }
    for (const auto &blocks : _actions) {
      for (auto block : blocks) {
        block->cleanup(block);
        block->destroy(block);
      }
    }
    destroyVar(_chains);
    stbds_arrfree(_chainValidation.exposedInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0: {
      destroy();
      _conditions.clear();
      _actions.clear();
      cloneVar(_chains, value);
      if (value.valueType == Seq) {
        auto counter = stbds_arrlen(value.payload.seqValue);
        if (counter % 2)
          throw CBException("Cond: first parameter must contain a sequence of "
                            "pairs [condition to check & action to perform if "
                            "check passed (true)].");
        _conditions.resize(counter / 2);
        _actions.resize(counter / 2);
        auto idx = 0;
        for (auto i = 0; i < counter; i++) {
          auto val = value.payload.seqValue[i];
          if (i % 2) { // action
            if (val.valueType == Block) {
              _actions[idx].push_back(val.payload.blockValue);
            } else { // seq
              for (auto y = 0; y < stbds_arrlen(val.payload.seqValue); y++) {
                assert(val.payload.seqValue[y].valueType == Block);
                _actions[idx].push_back(
                    val.payload.seqValue[y].payload.blockValue);
              }
            }

            idx++;
          } else { // condition
            if (val.valueType == Block) {
              _conditions[idx].push_back(val.payload.blockValue);
            } else { // seq
              for (auto y = 0; y < stbds_arrlen(val.payload.seqValue); y++) {
                assert(val.payload.seqValue[y].valueType == Block);
                _conditions[idx].push_back(
                    val.payload.seqValue[y].payload.blockValue);
              }
            }
          }
        }
      }
      break;
    }
    case 1:
      _passthrough = value.payload.boolValue;
      break;
    case 2:
      _threading = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _chains;
    case 1:
      return Var(_passthrough);
    case 2:
      return Var(_threading);
    default:
      break;
    }
    return Var();
  }

  CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    // Free any previous result!
    stbds_arrfree(_chainValidation.exposedInfo);
    _chainValidation.exposedInfo = nullptr;

    // Validate condition chains, altho they do not influence anything we need
    // to report errors
    for (const auto &action : _conditions) {
      auto validation = validateConnections(
          action,
          [](const CBlock *errorBlock, const char *errorTxt,
             bool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              LOG(ERROR) << "Cond: failed inner chain validation, error: "
                         << errorTxt;
              throw CBException("Cond: failed inner chain validation.");
            } else {
              LOG(INFO) << "Cond: warning during inner chain validation: "
                        << errorTxt;
            }
          },
          this, inputType, consumables);
      stbds_arrfree(validation.exposedInfo);
    }

    // Evaluate all actions, all must return the same type in order to be safe
    CBTypeInfo previousType{};
    auto idx = 0;
    for (const auto &action : _actions) {
      auto validation = validateConnections(
          action,
          [](const CBlock *errorBlock, const char *errorTxt,
             bool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              LOG(ERROR) << "Cond: failed inner chain validation, error: "
                         << errorTxt;
              throw CBException("Cond: failed inner chain validation.");
            } else {
              LOG(INFO) << "Cond: warning during inner chain validation: "
                        << errorTxt;
            }
          },
          this, inputType, consumables);

      if (!_chainValidation.exposedInfo) {
        // A first valid exposedInfo array is our gold
        _chainValidation = validation;
      } else {
        auto curlen = stbds_arrlen(_chainValidation.exposedInfo);
        auto newlen = stbds_arrlen(validation.exposedInfo);
        if (curlen != newlen) {
          throw CBException(
              "Cond: number of exposed variables between actions mismatch.");
        }
        for (auto i = 0; i < curlen; i++) {
          if (_chainValidation.exposedInfo[i] != validation.exposedInfo[i]) {
            throw CBException(
                "Cond: types of exposed variables between actions mismatch.");
          }
        }
      }
      if (idx > 0 && !_passthrough && validation.outputType != previousType)
        throw CBException("Cond: output types between actions mismatch.");
      idx++;
      previousType = validation.outputType;
    }

    return _passthrough ? inputType : previousType;
  }

  CBExposedTypesInfo exposedVariables() { return _chainValidation.exposedInfo; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(_actions.size() == 0)) {
      if (_passthrough)
        return input;
      else
        return CBVar(); // None
    }

    auto idx = 0;
    CBVar actionInput = input;
    CBVar finalOutput = RestartChain;
    for (const auto &cond : _conditions) {
      CBVar output{};
      if (unlikely(!activateBlocks(const_cast<CBlocks>(&cond[0]), cond.size(),
                                   context, input, output))) {
        return StopChain;
      } else if (output == True) {
        // Do the action if true!
        // And stop here
        output = Empty;
        auto state = activateBlocks(&_actions[idx][0], _actions[idx].size(),
                                    context, actionInput, output);
        if (unlikely(state == FlowState::Stopping)) {
          return StopChain;
        } else if (unlikely(state == FlowState::Returning)) {
          return Var::Return();
        } else if (_threading) {
          // set the output as the next action input (not cond tho!)
          finalOutput = output;
          actionInput = finalOutput;
        } else {
          // not _threading so short circuit here
          return _passthrough ? input : output;
        }
      }
      idx++;
    }

    return _passthrough ? input : finalOutput;
  }
};

// Register
RUNTIME_CORE_BLOCK(Cond);
RUNTIME_BLOCK_inputTypes(Cond);
RUNTIME_BLOCK_outputTypes(Cond);
RUNTIME_BLOCK_parameters(Cond);
RUNTIME_BLOCK_inferTypes(Cond);
RUNTIME_BLOCK_exposedVariables(Cond);
RUNTIME_BLOCK_setParam(Cond);
RUNTIME_BLOCK_getParam(Cond);
RUNTIME_BLOCK_activate(Cond);
RUNTIME_BLOCK_cleanup(Cond);
RUNTIME_BLOCK_destroy(Cond);
RUNTIME_BLOCK_END(Cond);

void registerFlowBlocks() { REGISTER_CORE_BLOCK(Cond); }
}; // namespace chainblocks
 {
    auto contents = _contents.get(context);
    if (contents.valueType != None) {
      fs::path p(input.payload.stringValue);
      if (!_overwrite && !_append && fs::exists(p)) {
        throw CBException(
            "FS.Write, file already exists and overwrite flag is not on!.");
      }

      auto flags = std::ios::binary;
      if (_append) {
        flags |= std::ios::app;
      }
      std::ofstream file(p.string(), flags);
      if (contents.valueType == String) {
        file << contents.payload.stringValue;
      } else {
        file.write((const char *)contents.payload.bytesValue,
                   contents.payload.bytesSize);
      }
    }
    return input;
  }
};

RUNTIME_BLOCK(FS, Write);
RUNTIME_BLOCK_inputTypes(Write);
RUNTIME_BLOCK_outputTypes(Write);
RUNTIME_BLOCK_parameters(Write);
RUNTIME_BLOCK_setParam(Write);
RUNTIME_BLOCK_getParam(Write);
RUNTIME_BLOCK_activate(Write);
RUNTIME_BLOCK_END(Write);
}; // namespace FS

void registerFSBlocks() {
  REGISTER_BLOCK(FS, Iterate);
  REGISTER_BLOCK(FS, Extension);
  REGISTER_BLOCK(FS, Filename);
  REGISTER_BLOCK(FS, Read);
  REGISTER_BLOCK(FS, Write);
  REGISTER_BLOCK(FS, IsFile);
  REGISTER_BLOCK(FS, IsDirectory);
}
}; // namespace chainblocksss", createBlockCross);
  chainblocks::registerBlock("Math.LinAlg.Dot", createBlockDot);
  chainblocks::registerBlock("Math.LinAlg.Normalize", createBlockNormalize);
  chainblocks::registerBlock("Math.LinAlg.LengthSquared",
                             createBlockLengthSquared);
  chainblocks::registerBlock("Math.LinAlg.Length", createBlockLength);
}
}; // namespace LinAlg
}; // namespace Math
}; // namespace chainblocks
utput.payload.float2Value =                                           \
            input.payload.float2Value OPERATOR operand.payload.float2Value;    \
        break;                                                                 \
      case Float3:                                                             \
        if constexpr (DIV_BY_ZERO) {                                           \
          for (auto i = 0; i < 3; i++)                                         \
            if (operand.payload.float3Value[i] == 0)                           \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Float3;                                             \
        output.payload.float3Value =                                           \
            input.payload.float3Value OPERATOR operand.payload.float3Value;    \
        break;                                                                 \
      case Float4:                                                             \
        if constexpr (DIV_BY_ZERO) {                                           \
          for (auto i = 0; i < 4; i++)                                         \
            if (operand.payload.float4Value[i] == 0)                           \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Float4;                                             \
        output.payload.float4Value =                                           \
            input.payload.float4Value OPERATOR operand.payload.float4Value;    \
        break;                                                                 \
      case Color:                                                              \
        if constexpr (DIV_BY_ZERO) {                                           \
          if (operand.payload.colorValue.r == 0)                               \
            throw CBException("Error, division by 0!");                        \
          if (operand.payload.colorValue.g == 0)                               \
            throw CBException("Error, division by 0!");                        \
          if (operand.payload.colorValue.b == 0)                               \
            throw CBException("Error, division by 0!");                        \
          if (operand.payload.colorValue.a == 0)                               \
            throw CBException("Error, division by 0!");                        \
        }                                                                      \
        output.valueType = Color;                                              \
        output.payload.colorValue.r =                                          \
            input.payload.colorValue.r OPERATOR operand.payload.colorValue.r;  \
        output.payload.colorValue.g =                                          \
            input.payload.colorValue.g OPERATOR operand.payload.colorValue.g;  \
        output.payload.colorValue.b =                                          \
            input.payload.colorValue.b OPERATOR operand.payload.colorValue.b;  \
        output.payload.colorValue.a =                                          \
            input.payload.colorValue.a OPERATOR operand.payload.colorValue.a;  \
        break;                                                                 \
      default:                                                                 \
        throw CBException(OPERATOR_STR                                         \
                          " operation not supported between given types!");    \
      }                                                                        \
    }                                                                          \
                                                                               \
    ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {     \
      if (_operand.valueType == ContextVar && _ctxOperand == nullptr) {        \
        _ctxOperand = contextVariable(context, _operand.payload.stringValue);  \
      }                                                                        \
      auto &operand = _ctxOperand ? *_ctxOperand : _operand;                   \
      CBVar output{};                                                          \
      if (likely(_opType == Normal)) {                                         \
        operate(output, input, operand);                                       \
        return output;                                                         \
      } else if (_opType == Seq1) {                                            \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {      \
          operate(output, input.payload.seqValue[i], operand);                 \
          stbds_arrpush(_cachedSeq.payload.seqValue, output);                  \
        }                                                                      \
        return _cachedSeq;                                                     \
      } else {                                                                 \
        auto olen = stbds_arrlen(operand.payload.seqValue);                    \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {      \
          operate(output, input.payload.seqValue[i],                           \
                  operand.payload.seqValue[i % olen]);                         \
          stbds_arrpush(_cachedSeq.payload.seqValue, output);                  \
        }                                                                      \
        return _cachedSeq;                                                     \
      }                                                                        \
    }                                                                          \
  };                                                                           \
  RUNTIME_BLOCK_TYPE(Math, NAME);

#define MATH_BINARY_INT_OPERATION(NAME, OPERATOR, OPERATOR_STR)                \
  struct NAME : public BinaryBase {                                            \
    ALWAYS_INLINE void operate(CBVar &output, const CBVar &input,              \
                               const CBVar &operand) {                         \
      if (input.valueType != operand.valueType)                                \
        throw CBException("Operation not supported between different "         \
                          "types.");                                           \
      switch (input.valueType) {                                               \
      case Int:                                                                \
        output.valueType = Int;                                                \
        output.payload.intValue =                                              \
            input.payload.intValue OPERATOR operand.payload.intValue;          \
        break;                                                                 \
      case Int2:                                                               \
        output.valueType = Int2;                                               \
        output.payload.int2Value =                                             \
            input.payload.int2Value OPERATOR operand.payload.int2Value;        \
        break;                                                                 \
      case Int3:                                                               \
        output.valueType = Int3;                                               \
        output.payload.int3Value =                                             \
            input.payload.int3Value OPERATOR operand.payload.int3Value;        \
        break;                                                                 \
      case Int4:                                                               \
        output.valueType = Int4;                                               \
        output.payload.int4Value =                                             \
            input.payload.int4Value OPERATOR operand.payload.int4Value;        \
        break;                                                                 \
      case Int8:                                                               \
        output.valueType = Int8;                                               \
        output.payload.int8Value =                                             \
            input.payload.int8Value OPERATOR operand.payload.int8Value;        \
        break;                                                                 \
      case Int16:                                                              \
        output.valueType = Int16;                                              \
        output.payload.int16Value =                                            \
            input.payload.int16Value OPERATOR operand.payload.int16Value;      \
        break;                                                                 \
      case Color:                                                              \
        output.valueType = Color;                                              \
        output.payload.colorValue.r =                                          \
            input.payload.colorValue.r OPERATOR operand.payload.colorValue.r;  \
        output.payload.colorValue.g =                                          \
            input.payload.colorValue.g OPERATOR operand.payload.colorValue.g;  \
        output.payload.colorValue.b =                                          \
            input.payload.colorValue.b OPERATOR operand.payload.colorValue.b;  \
        output.payload.colorValue.a =                                          \
            input.payload.colorValue.a OPERATOR operand.payload.colorValue.a;  \
        break;                                                                 \
      default:                                                                 \
        throw CBException(OPERATOR_STR                                         \
                          " operation not supported between given types!");    \
      }                                                                        \
    }                                                                          \
                                                                               \
    ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {     \
      if (_operand.valueType == ContextVar && _ctxOperand == nullptr) {        \
        _ctxOperand = contextVariable(context, _operand.payload.stringValue);  \
      }                                                                        \
      auto &operand = _ctxOperand ? *_ctxOperand : _operand;                   \
      CBVar output{};                                                          \
      if (likely(_opType == Normal)) {                                         \
        operate(output, input, operand);                                       \
        return output;                                                         \
      } else if (_opType == Seq1) {                                            \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {      \
          operate(output, input.payload.seqValue[i], operand);                 \
          stbds_arrpush(_cachedSeq.payload.seqValue, output);                  \
        }                                                                      \
        return _cachedSeq;                                                     \
      } else {                                                                 \
        auto olen = stbds_arrlen(operand.payload.seqValue);                    \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {      \
          operate(output, input.payload.seqValue[i],                           \
                  operand.payload.seqValue[i % olen]);                         \
          stbds_arrpush(_cachedSeq.payload.seqValue, output);                  \
        }                                                                      \
        return _cachedSeq;                                                     \
      }                                                                        \
    }                                                                          \
  };                                                                           \
  RUNTIME_BLOCK_TYPE(Math, NAME);

MATH_BINARY_OPERATION(Add, +, "Add", 0);
MATH_BINARY_OPERATION(Subtract, -, "Subtract", 0);
MATH_BINARY_OPERATION(Multiply, *, "Multiply", 0);
MATH_BINARY_OPERATION(Divide, /, "Divide", 1);
MATH_BINARY_INT_OPERATION(Xor, ^, "Xor");
MATH_BINARY_INT_OPERATION(And, &, "And");
MATH_BINARY_INT_OPERATION(Or, |, "Or");
MATH_BINARY_INT_OPERATION(Mod, %, "Mod");
MATH_BINARY_INT_OPERATION(LShift, <<, "LShift");
MATH_BINARY_INT_OPERATION(RShift, >>, "RShift");

#define MATH_BINARY_BLOCK(NAME)                                                \
  RUNTIME_BLOCK_FACTORY(Math, NAME);                                           \
  RUNTIME_BLOCK_destroy(NAME);                                                 \
  RUNTIME_BLOCK_cleanup(NAME);                                                 \
  RUNTIME_BLOCK_setup(NAME);                                                   \
  RUNTIME_BLOCK_inputTypes(NAME);                                              \
  RUNTIME_BLOCK_outputTypes(NAME);                                             \
  RUNTIME_BLOCK_parameters(NAME);                                              \
  RUNTIME_BLOCK_inferTypes(NAME);                                              \
  RUNTIME_BLOCK_consumedVariables(NAME);                                       \
  RUNTIME_BLOCK_setParam(NAME);                                                \
  RUNTIME_BLOCK_getParam(NAME);                                                \
  RUNTIME_BLOCK_activate(NAME);                                                \
  RUNTIME_BLOCK_END(NAME);

#define MATH_UNARY_OPERATION(NAME, FUNC, FUNCF, FUNC_STR)                      \
  struct NAME : public UnaryBase {                                             \
    ALWAYS_INLINE void operate(CBVar &output, const CBVar &input) {            \
      switch (input.valueType) {                                               \
      case Float:                                                              \
        output.valueType = Float;                                              \
        output.payload.floatValue = FUNC(input.payload.floatValue);            \
        break;                                                                 \
      case Float2:                                                             \
        output.valueType = Float2;                                             \
        output.payload.float2Value[0] = FUNC(input.payload.float2Value[0]);    \
        output.payload.float2Value[1] = FUNC(input.payload.float2Value[1]);    \
        break;                                                                 \
      case Float3:                                                             \
        output.valueType = Float3;                                             \
        output.payload.float3Value[0] = FUNCF(input.payload.float3Value[0]);   \
        output.payload.float3Value[1] = FUNCF(input.payload.float3Value[1]);   \
        output.payload.float3Value[2] = FUNCF(input.payload.float3Value[2]);   \
        break;                                                                 \
      case Float4:                                                             \
        output.valueType = Float4;                                             \
        output.payload.float4Value[0] = FUNCF(input.payload.float4Value[0]);   \
        output.payload.float4Value[1] = FUNCF(input.payload.float4Value[1]);   \
        output.payload.float4Value[2] = FUNCF(input.payload.float4Value[2]);   \
        output.payload.float4Value[3] = FUNCF(input.payload.float4Value[3]);   \
        break;                                                                 \
      default:                                                                 \
        throw CBException(FUNC_STR                                             \
                          " operation not supported between given types!");    \
      }                                                                        \
    }                                                                          \
                                                                               \
    ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {     \
      CBVar output{};                                                          \
      if (unlikely(input.valueType == Seq)) {                                  \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {      \
          operate(output, input);                                              \
          stbds_arrpush(_cachedSeq.payload.seqValue, output);                  \
        }                                                                      \
        return _cachedSeq;                                                     \
      } else {                                                                 \
        operate(output, input);                                                \
        return output;                                                         \
      }                                                                        \
    }                                                                          \
  };                                                                           \
  RUNTIME_BLOCK_TYPE(Math, NAME);

MATH_UNARY_OPERATION(Abs, __builtin_fabs, __builtin_fabsf, "Abs");
MATH_UNARY_OPERATION(Exp, __builtin_exp, __builtin_expf, "Exp");
MATH_UNARY_OPERATION(Exp2, __builtin_exp2, __builtin_exp2f, "Exp2");
MATH_UNARY_OPERATION(Expm1, __builtin_expm1, __builtin_expm1f, "Expm1");
MATH_UNARY_OPERATION(Log, __builtin_log, __builtin_logf, "Log");
MATH_UNARY_OPERATION(Log10, __builtin_log10, __builtin_log10f, "Log10");
MATH_UNARY_OPERATION(Log2, __builtin_log2, __builtin_log2f, "Log2");
MATH_UNARY_OPERATION(Log1p, __builtin_log1p, __builtin_log1pf, "Log1p");
MATH_UNARY_OPERATION(Sqrt, __builtin_sqrt, __builtin_sqrtf, "Sqrt");
MATH_UNARY_OPERATION(Cbrt, __builtin_cbrt, __builtin_cbrt, "Cbrt");
MATH_UNARY_OPERATION(Sin, __builtin_sin, __builtin_sinf, "Sin");
MATH_UNARY_OPERATION(Cos, __builtin_cos, __builtin_cosf, "Cos");
MATH_UNARY_OPERATION(Tan, __builtin_tan, __builtin_tanf, "Tan");
MATH_UNARY_OPERATION(Asin, __builtin_asin, __builtin_asinf, "Asin");
MATH_UNARY_OPERATION(Acos, __builtin_acos, __builtin_acosf, "Acos");
MATH_UNARY_OPERATION(Atan, __builtin_atan, __builtin_atanf, "Atan");
MATH_UNARY_OPERATION(Sinh, __builtin_sinh, __builtin_sinhf, "Sinh");
MATH_UNARY_OPERATION(Cosh, __builtin_cosh, __builtin_coshf, "Cosh");
MATH_UNARY_OPERATION(Tanh, __builtin_tanh, __builtin_tanhf, "Tanh");
MATH_UNARY_OPERATION(Asinh, __builtin_asinh, __builtin_asinhf, "Asinh");
MATH_UNARY_OPERATION(Acosh, __builtin_acosh, __builtin_acoshf, "Acosh");
MATH_UNARY_OPERATION(Atanh, __builtin_atanh, __builtin_atanhf, "Atanh");
MATH_UNARY_OPERATION(Erf, __builtin_erf, __builtin_erff, "Erf");
MATH_UNARY_OPERATION(Erfc, __builtin_erfc, __builtin_erfcf, "Erfc");
MATH_UNARY_OPERATION(TGamma, __builtin_tgamma, __builtin_tgammaf, "TGamma");
MATH_UNARY_OPERATION(LGamma, __builtin_lgamma, __builtin_lgammaf, "LGamma");
MATH_UNARY_OPERATION(Ceil, __builtin_ceil, __builtin_ceilf, "Ceil");
MATH_UNARY_OPERATION(Floor, __builtin_floor, __builtin_floorf, "Floor");
MATH_UNARY_OPERATION(Trunc, __builtin_trunc, __builtin_truncf, "Trunc");
MATH_UNARY_OPERATION(Round, __builtin_round, __builtin_roundf, "Round");

#define MATH_UNARY_BLOCK(NAME)                                                 \
  RUNTIME_BLOCK_FACTORY(Math, NAME);                                           \
  RUNTIME_BLOCK_destroy(NAME);                                                 \
  RUNTIME_BLOCK_setup(NAME);                                                   \
  RUNTIME_BLOCK_inputTypes(NAME);                                              \
  RUNTIME_BLOCK_outputTypes(NAME);                                             \
  RUNTIME_BLOCK_activate(NAME);                                                \
  RUNTIME_BLOCK_END(NAME);

struct Mean {
  static CBTypesInfo inputTypes() {
    return CBTypesInfo(CoreInfo::floatSeqInfo);
  }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::floatInfo); }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    int64_t inputLen = stbds_arrlen(input.payload.seqValue);
    double mean = 0.0;
    auto seq = IterableSeq(input.payload.seqValue);
    for (auto &f : seq) {
      mean += f.payload.floatValue;
    }
    mean /= double(inputLen);
    return Var(mean);
  }
};
}; // namespace Math
}; // namespace chainblocks
      var.payload.chainValue = new CBChain(chainName.c_str());
      chainblocks::GlobalChains[chainName] = var.payload.chainValue;
    }
    break;
  }
  case Block: {
    var.valueType = Block;
    auto blkname = j.at("name").get<std::string>();
    auto blk = chainblocks::createBlock(blkname.c_str());
    if (!blk) {
      auto errmsg = "Failed to create block of type: " + std::string("blkname");
      throw chainblocks::CBException(errmsg.c_str());
    }
    var.payload.blockValue = blk;

    // Setup
    blk->setup(blk);

    // Set params
    auto jparams = j.at("params");
    auto blkParams = blk->parameters(blk);
    for (auto jparam : jparams) {
      auto paramName = jparam.at("name").get<std::string>();
      auto value = jparam.at("value").get<CBVar>();

      if (value.valueType != None) {
        for (auto i = 0; stbds_arrlen(blkParams) > i; i++) {
          auto &paramInfo = blkParams[i];
          if (paramName == paramInfo.name) {
            blk->setParam(blk, i, value);
            break;
          }
        }
      }

      // Assume block copied memory internally so we can clean up here!!!
      releaseMemory(value);
    }
    break;
  }
  }
}

void to_json(json &j, const CBChainPtr &chain) {
  std::vector<json> blocks;
  for (auto blk : chain->blocks) {
    std::vector<json> params;
    auto paramsDesc = blk->parameters(blk);
    for (int i = 0; stbds_arrlen(paramsDesc) > i; i++) {
      auto &desc = paramsDesc[i];
      auto value = blk->getParam(blk, i);

      json param_obj = {{"name", desc.name}, {"value", value}};

      params.push_back(param_obj);
    }

    json block_obj = {{"name", blk->name(blk)}, {"params", params}};

    blocks.push_back(block_obj);
  }

  j = {
      {"blocks", blocks},        {"name", chain->name},
      {"looped", chain->looped}, {"unsafe", chain->unsafe},
      {"version", 0.1},
  };
}

void from_json(const json &j, CBChainPtr &chain) {
  auto chainName = j.at("name").get<std::string>();
  auto findIt = chainblocks::GlobalChains.find(chainName);
  if (findIt != chainblocks::GlobalChains.end()) {
    chain = findIt->second;
    // Need to clean it up for rewrite!
    chain->cleanup();
  } else {
    chain = new CBChain(chainName.c_str());
    chainblocks::GlobalChains[chainName] = chain;
  }

  chain->looped = j.at("looped").get<bool>();
  chain->unsafe = j.at("unsafe").get<bool>();

  auto jblocks = j.at("blocks");
  for (auto jblock : jblocks) {
    auto blkname = jblock.at("name").get<std::string>();
    auto blk = chainblocks::createBlock(blkname.c_str());
    if (!blk) {
      auto errmsg = "Failed to create block of type: " + std::string(blkname);
      throw chainblocks::CBException(errmsg.c_str());
    }

    // Setup
    blk->setup(blk);

    // Set params
    auto jparams = jblock.at("params");
    auto blkParams = blk->parameters(blk);
    for (auto jparam : jparams) {
      auto paramName = jparam.at("name").get<std::string>();
      auto value = jparam.at("value").get<CBVar>();

      if (value.valueType != None) {
        for (auto i = 0; stbds_arrlen(blkParams) > i; i++) {
          auto &paramInfo = blkParams[i];
          if (paramName == paramInfo.name) {
            blk->setParam(blk, i, value);
            break;
          }
        }
      }

      // Assume block copied memory internally so we can clean up here!!!
      releaseMemory(value);
    }

    // From now on this chain owns the block
    chain->addBlock(blk);
  }
}

bool matchTypes(const CBTypeInfo &exposedType, const CBTypeInfo &consumedType,
                bool isParameter, bool strict) {
  if (consumedType.basicType == Any ||
      (!isParameter && consumedType.basicType == None))
    return true;

  if (exposedType.basicType != consumedType.basicType) {
    // Fail if basic type differs
    return false;
  }

  switch (exposedType.basicType) {
  case Object: {
    if (exposedType.objectVendorId != consumedType.objectVendorId ||
        exposedType.objectTypeId != consumedType.objectTypeId) {
      return false;
    }
    break;
  }
  case Enum: {
    if (exposedType.enumVendorId != consumedType.enumVendorId ||
        exposedType.enumTypeId != consumedType.enumTypeId) {
      return false;
    }
    break;
  }
  case Seq: {
    if (strict) {
      if (exposedType.seqType && consumedType.seqType) {
        if (!matchTypes(*exposedType.seqType, *consumedType.seqType,
                        isParameter, strict)) {
          return false;
        }
      } else if (exposedType.seqType == nullptr ||
                 consumedType.seqType == nullptr) {
        return false;
      } else if (exposedType.seqType && consumedType.seqType == nullptr &&
                 !isParameter) {
        // Assume consumer is not strict
        return true;
      }
    }
    break;
  }
  case Table: {
    if (strict) {
      auto atypes = stbds_arrlen(exposedType.tableTypes);
      auto btypes = stbds_arrlen(consumedType.tableTypes);
      //  btypes != 0 assume consumer is not strict
      for (auto i = 0; i < atypes && (isParameter || btypes != 0); i++) {
        // Go thru all exposed types and make sure we get a positive match with
        // the consumer
        auto atype = exposedType.tableTypes[i];
        auto matched = false;
        for (auto y = 0; y < btypes; y++) {
          auto btype = consumedType.tableTypes[y];
          if (matchTypes(atype, btype, isParameter, strict)) {
            matched = true;
            break;
          }
        }
        if (!matched) {
          return false;
        }
      }
    }
    break;
  }
  default:
    return true;
  }
  return true;
}

namespace std {
template <> struct hash<CBExposedTypeInfo> {
  std::size_t operator()(const CBExposedTypeInfo &typeInfo) const {
    using std::hash;
    using std::size_t;
    using std::string;
    auto res = hash<string>()(typeInfo.name);
    res = res ^ hash<int>()(typeInfo.exposedType.basicType);
    res = res ^ hash<int>()(typeInfo.isMutable);
    if (typeInfo.exposedType.basicType == Table &&
        typeInfo.exposedType.tableTypes && typeInfo.exposedType.tableKeys) {
      for (auto i = 0; i < stbds_arrlen(typeInfo.exposedType.tableKeys); i++) {
        res = res ^ hash<string>()(typeInfo.exposedType.tableKeys[i]);
      }
    } else if (typeInfo.exposedType.basicType == Seq &&
               typeInfo.exposedType.seqType) {
      res = res ^ hash<int>()(typeInfo.exposedType.seqType->basicType);
    }
    return res;
  }
};
} // namespace std

struct ValidationContext {
  phmap::flat_hash_map<std::string, phmap::flat_hash_set<CBExposedTypeInfo>>
      exposed;
  phmap::flat_hash_set<std::string> variables;
  phmap::flat_hash_set<std::string> references;

  CBTypeInfo previousOutputType{};
  CBTypeInfo originalInputType{};

  CBlock *bottom{};

  CBValidationCallback cb{};
  void *userData{};
};

struct ConsumedParam {
  const char *key;
  CBParameterInfo value;
};

void validateConnection(ValidationContext &ctx) {
  auto previousOutput = ctx.previousOutputType;

  auto inputInfos = ctx.bottom->inputTypes(ctx.bottom);
  auto inputMatches = false;
  // validate our generic input
  for (auto i = 0; stbds_arrlen(inputInfos) > i; i++) {
    auto &inputInfo = inputInfos[i];
    if (matchTypes(previousOutput, inputInfo, false, false)) {
      inputMatches = true;
      break;
    }
  }

  if (!inputMatches) {
    std::string err("Could not find a matching input type, block: " +
                    std::string(ctx.bottom->name(ctx.bottom)));
    ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
  }

  // infer and specialize types if we need to
  // If we don't we assume our output will be of the same type of the previous!
  if (ctx.bottom->inferTypes) {
    CBExposedTypesInfo consumables = nullptr;
    // Pass all we got in the context!
    for (auto &info : ctx.exposed) {
      for (auto &type : info.second) {
        stbds_arrpush(consumables, type);
      }
    }
    // this ensures e.g. SetVariable exposedVars have right type from the actual
    // input type (previousOutput)!
    ctx.previousOutputType =
        ctx.bottom->inferTypes(ctx.bottom, previousOutput, consumables);

    stbds_arrfree(consumables);
  } else {
    // Short-cut if it's just one type and not any type
    // Any type tho means keep previous output type!
    auto outputTypes = ctx.bottom->outputTypes(ctx.bottom);
    if (stbds_arrlen(outputTypes) == 1 && outputTypes[0].basicType != Any) {
      ctx.previousOutputType = outputTypes[0];
    }
  }

  // Grab those after type inference!
  auto exposedVars = ctx.bottom->exposedVariables(ctx.bottom);
  // Add the vars we expose
  for (auto i = 0; stbds_arrlen(exposedVars) > i; i++) {
    auto &exposed_param = exposedVars[i];
    std::string name(exposed_param.name);
    ctx.exposed[name].insert(exposed_param);

    // Reference mutability checks
    if (strcmp(ctx.bottom->name(ctx.bottom), "Ref") == 0) {
      // make sure we are not Ref-ing a Set
      // meaning target would be overwritten, yet Set will try to deallocate
      // it/manage it
      if (ctx.variables.contains(name)) {
        // Error
        std::string err(
            "Ref variable name already used as Set. Overwriting a previously "
            "Set variable with Ref is not allowed, name: " +
            name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.references.insert(name);
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Set") == 0) {
      // make sure we are not Set-ing a Ref
      // meaning target memory could be any block temporary buffer, yet Set will
      // try to deallocate it/manage it
      if (ctx.references.contains(name)) {
        // Error
        std::string err(
            "Set variable name already used as Ref. Overwriting a previously "
            "Ref variable with Set is not allowed, name: " +
            name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.variables.insert(name);
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Update") == 0) {
      // make sure we are not Set-ing a Ref
      // meaning target memory could be any block temporary buffer, yet Set will
      // try to deallocate it/manage it
      if (ctx.references.contains(name)) {
        // Error
        std::string err("Update variable name already used as Ref. Overwriting "
                        "a previously "
                        "Ref variable with Update is not allowed, name: " +
                        name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Push") == 0) {
      // make sure we are not Push-ing a Ref
      // meaning target memory could be any block temporary buffer, yet Push
      // will try to deallocate it/manage it
      if (ctx.references.contains(name)) {
        // Error
        std::string err(
            "Push variable name already used as Ref. Overwriting a previously "
            "Ref variable with Push is not allowed, name: " +
            name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.variables.insert(name);
    }
  }

  // Finally do checks on what we consume
  auto consumedVar = ctx.bottom->consumedVariables(ctx.bottom);

  phmap::node_hash_map<std::string, std::vector<CBExposedTypeInfo>>
      consumedVars;
  for (auto i = 0; stbds_arrlen(consumedVar) > i; i++) {
    auto &consumed_param = consumedVar[i];
    std::string name(consumed_param.name);
    consumedVars[name].push_back(consumed_param);
  }

  // make sure we have the vars we need, collect first
  for (const auto &consumed : consumedVars) {
    auto matching = false;

    for (const auto &consumed_param : consumed.second) {
      std::string name(consumed_param.name);
      if (name.find(' ') != -1) { // take only the first part of variable name
        // the remaining should be a table key which we don't care here
        name = name.substr(0, name.find(' '));
      }
      auto findIt = ctx.exposed.find(name);
      if (findIt == ctx.exposed.end()) {
        std::string err("Required consumed variable not found: " + name);
        // Warning only, delegate inferTypes to decide
        ctx.cb(ctx.bottom, err.c_str(), true, ctx.userData);
      } else {

        for (auto type : findIt->second) {
          auto exposedType = type.exposedType;
          auto requiredType = consumed_param.exposedType;
          // Finally deep compare types
          if (matchTypes(exposedType, requiredType, false, true)) {
            matching = true;
            break;
          }
        }
      }
      if (matching)
        break;
    }

    if (!matching) {
      std::string err(
          "Required consumed types do not match currently exposed ones: " +
          consumed.first);
      err += " exposed types:";
      for (auto info : ctx.exposed) {
        err += " (" + info.first + " [";

        for (auto type : info.second) {
          if (type.exposedType.basicType == Table &&
              type.exposedType.tableTypes && type.exposedType.tableKeys) {
            err += "(" + type2Name(type.exposedType.basicType) + " [" +
                   type2Name(type.exposedType.tableTypes[0].basicType) + " " +
                   type.exposedType.tableKeys[0] + "]) ";
          } else if (type.exposedType.basicType == Seq &&
                     type.exposedType.seqType) {
            err += "(" + type2Name(type.exposedType.basicType) + " [" +
                   type2Name(type.exposedType.seqType->basicType) + "]) ";
          } else {
            err += type2Name(type.exposedType.basicType) + " ";
          }
        }
        err.erase(err.end() - 1);

        err += "])";
      }
      ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
    }
  }
}

CBValidationResult validateConnections(const std::vector<CBlock *> &chain,
                                       CBValidationCallback callback,
                                       void *userData, CBTypeInfo inputType,
                                       CBExposedTypesInfo consumables) {
  auto ctx = ValidationContext();
  ctx.originalInputType = inputType;
  ctx.previousOutputType = inputType;
  ctx.cb = callback;
  ctx.userData = userData;

  if (consumables) {
    for (auto i = 0; i < stbds_arrlen(consumables); i++) {
      auto &info = consumables[i];
      ctx.exposed[info.name].insert(info);
    }
  }

  for (auto blk : chain) {

    if (strcmp(blk->name(blk), "Input") == 0 ||
        strcmp(blk->name(blk), "And") == 0 ||
        strcmp(blk->name(blk), "Or") == 0) {
      // Hard code behavior for Input block and And and Or, in order to validate
      // with actual chain input the followup
      ctx.previousOutputType = ctx.originalInputType;
    } else {
      ctx.bottom = blk;
      validateConnection(ctx);
    }
  }

  CBValidationResult result = {ctx.previousOutputType};
  for (auto &exposed : ctx.exposed) {
    for (auto &type : exposed.second) {
      stbds_arrpush(result.exposedInfo, type);
    }
  }
  return result;
}

CBValidationResult validateConnections(const CBChain *chain,
                                       CBValidationCallback callback,
                                       void *userData, CBTypeInfo inputType,
                                       CBExposedTypesInfo consumables) {
  return validateConnections(chain->blocks, callback, userData, inputType,
                             consumables);
}

CBValidationResult validateConnections(const CBlocks chain,
                                       CBValidationCallback callback,
                                       void *userData, CBTypeInfo inputType,
                                       CBExposedTypesInfo consumables) {
  std::vector<CBlock *> blocks;
  for (auto i = 0; stbds_arrlen(chain) > i; i++) {
    blocks.push_back(chain[i]);
  }
  return validateConnections(blocks, callback, userData, inputType,
                             consumables);
}

void freeDerivedInfo(CBTypeInfo info) {
  switch (info.basicType) {
  case Seq: {
    freeDerivedInfo(*info.seqType);
    delete info.seqType;
  }
  case Table: {
    for (auto i = 0; stbds_arrlen(info.tableTypes) > i; i++) {
      freeDerivedInfo(info.tableTypes[i]);
    }
    stbds_arrfree(info.tableTypes);
  }
  default:
    break;
  };
}

CBTypeInfo deriveTypeInfo(CBVar &value) {
  // We need to guess a valid CBTypeInfo for this var in order to validate
  // Build a CBTypeInfo for the var
  auto varType = CBTypeInfo();
  varType.basicType = value.valueType;
  varType.seqType = nullptr;
  varType.tableTypes = nullptr;
  switch (value.valueType) {
  case Object: {
    varType.objectVendorId = value.payload.objectVendorId;
    varType.objectTypeId = value.payload.objectTypeId;
    break;
  }
  case Enum: {
    varType.enumVendorId = value.payload.enumVendorId;
    varType.enumTypeId = value.payload.enumTypeId;
    break;
  }
  case Seq: {
    varType.seqType = new CBTypeInfo();
    if (stbds_arrlen(value.payload.seqValue) > 0) {
      *varType.seqType = deriveTypeInfo(value.payload.seqValue[0]);
    }
    break;
  }
  case Table: {
    for (auto i = 0; stbds_arrlen(value.payload.tableValue) > i; i++) {
      stbds_arrpush(varType.tableTypes,
                    deriveTypeInfo(value.payload.tableValue[i].value));
    }
    break;
  }
  default:
    break;
  };
  return varType;
}

bool validateSetParam(CBlock *block, int index, CBVar &value,
                      CBValidationCallback callback, void *userData) {
  auto params = block->parameters(block);
  if (stbds_arrlen(params) <= index) {
    std::string err("Parameter index out of range");
    callback(block, err.c_str(), false, userData);
    return false;
  }

  auto param = params[index];

  // Build a CBTypeInfo for the var
  auto varType = deriveTypeInfo(value);

  for (auto i = 0; stbds_arrlen(param.valueTypes) > i; i++) {
    if (matchTypes(varType, param.valueTypes[i], true, true)) {
      freeDerivedInfo(varType);
      return true; // we are good just exit
    }
  }

  // Failed until now but let's check if the type is a sequenced too
  if (value.valueType == Seq) {
    // Validate each type in the seq
    for (auto i = 0; stbds_arrlen(value.payload.seqValue) > i; i++) {
      if (validateSetParam(block, index, value.payload.seqValue[i], callback,
                           userData)) {
        freeDerivedInfo(varType);
        return true;
      }
    }
  }

  std::string err("Parameter not accepting this kind of variable");
  callback(block, err.c_str(), false, userData);

  freeDerivedInfo(varType);

  return false;
}

void CBChain::cleanup() {
  if (node) {
    node->remove(this);
    node = nullptr;
  }

  for (auto blk : blocks) {
    blk->cleanup(blk);
    blk->destroy(blk);
    // blk is responsible to free itself, as they might use any allocation
    // strategy they wish!
  }
  blocks.clear();

  if (ownedOutput) {
    chainblocks::destroyVar(finishedOutput);
    ownedOutput = false;
  }
}

namespace chainblocks {
void error_handler(int err_sig) {
  std::signal(err_sig, SIG_DFL);

  auto printTrace = false;

  switch (err_sig) {
  case SIGINT:
  case SIGTERM:
    break;
  case SIGFPE:
    LOG(ERROR) << "Fatal SIGFPE";
    printTrace = true;
    break;
  case SIGILL:
    LOG(ERROR) << "Fatal SIGILL";
    printTrace = true;
    break;
  case SIGABRT:
    LOG(ERROR) << "Fatal SIGABRT";
    printTrace = true;
    break;
  case SIGSEGV:
    LOG(ERROR) << "Fatal SIGSEGV";
    printTrace = true;
    break;
  }

  if (printTrace) {
    std::signal(SIGABRT, SIG_DFL); // also make sure to remove this due to
                                   // logger double report on FATAL
    LOG(ERROR) << boost::stacktrace::stacktrace();
  }

  std::raise(err_sig);
}

void installSignalHandlers() {
  std::signal(SIGFPE, &error_handler);
  std::signal(SIGILL, &error_handler);
  std::signal(SIGABRT, &error_handler);
  std::signal(SIGSEGV, &error_handler);
}
}; // namespace chainblocks

#ifdef TESTING
static CBChain mainChain("MainChain");

int main() {
  auto blk = chainblocks::createBlock("SetTableValue");
  LOG(INFO) << blk->name(blk);
  LOG(INFO) << blk->exposedVariables(blk);
}
#endif
#pragma once

#include "core.hpp"

namespace chainblocks {
namespace Math {
struct Base {
  static inline TypesInfo mathTypesInfo = TypesInfo::FromMany(
      true, CBType::Int, CBType::Int2, CBType::Int3, CBType::Int4, CBType::Int8,
      CBType::Int16, CBType::Float, CBType::Float2, CBType::Float3,
      CBType::Float4, CBType::Color);

  CBVar _cachedSeq{};

  static CBTypesInfo inputTypes() { return CBTypesInfo(mathTypesInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(mathTypesInfo); }
};

struct UnaryBase : public Base {
  void destroy() {
    if (_cachedSeq.valueType == Seq) {
      stbds_arrfree(_cachedSeq.payload.seqValue);
    }
  }

  void setup() {
    _cachedSeq.valueType = Seq;
    _cachedSeq.payload.seqValue = nullptr;
    _cachedSeq.payload.seqLen = -1;
  }
};

struct BinaryBase : public Base {
  enum OpType { Normal, Seq1, SeqSeq };

  static inline TypesInfo mathBaseTypesInfo1 = TypesInfo::FromMany(
      true, CBType::Int, CBType::Int2, CBType::Int3, CBType::Int4, CBType::Int8,
      CBType::Int16, CBType::Float, CBType::Float2, CBType::Float3,
      CBType::Float4, CBType::Color, CBType::ContextVar);
  static inline ParamsInfo mathParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Operand", "The operand.", CBTypesInfo(mathBaseTypesInfo1)));

  CBVar _operand{};
  CBVar *_ctxOperand{};
  ExposedInfo _consumedInfo{};
  OpType _opType = Normal;

  void cleanup() { _ctxOperand = nullptr; }

  void destroy() {
    if (_cachedSeq.valueType == Seq) {
      stbds_arrfree(_cachedSeq.payload.seqValue);
    }
    destroyVar(_operand);
  }

  void setup() {
    _cachedSeq.valueType = Seq;
    _cachedSeq.payload.seqValue = nullptr;
    _cachedSeq.payload.seqLen = -1;
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(mathParamsInfo);
  }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    if (_operand.valueType == ContextVar) {
      for (auto i = 0; i < stbds_arrlen(consumableVariables); i++) {
        if (consumableVariables[i].name == _operand.payload.stringValue) {
          if (consumableVariables[i].exposedType.basicType != Seq &&
              inputType.basicType != Seq) {
            _opType = Normal;
          } else if (consumableVariables[i].exposedType.basicType != Seq &&
                     inputType.basicType == Seq) {
            _opType = Seq1;
          } else if (consumableVariables[i].exposedType.basicType == Seq &&
                     inputType.basicType == Seq) {
            _opType = SeqSeq;
          } else {
            throw CBException(
                "Math broadcasting not supported between given types!");
          }
        }
      }
    } else {
      if (_operand.valueType != Seq && inputType.basicType != Seq) {
        _opType = Normal;
      } else if (_operand.valueType != Seq && inputType.basicType == Seq) {
        _opType = Seq1;
      } else if (_operand.valueType == Seq && inputType.basicType == Seq) {
        _opType = SeqSeq;
      } else {
        throw CBException(
            "Math broadcasting not supported between given types!");
      }
    }
    return inputType;
  }

  CBExposedTypesInfo consumedVariables() {
    if (_operand.valueType == ContextVar) {
      _consumedInfo = ExposedInfo(ExposedInfo::Variable(
          _operand.payload.stringValue, "The consumed operand.",
          CBTypeInfo(CoreInfo::anyInfo)));
      return CBExposedTypesInfo(_consumedInfo);
    }
    return nullptr;
  }

  void setParam(int index, CBVar value) {
    cloneVar(_operand, value);
    cleanup();
  }

  CBVar getParam(int index) { return _operand; }
};

#define MATH_BINARY_OPERATION(NAME, OPERATOR, OPERATOR_STR, DIV_BY_ZERO)       \
  struct NAME : public BinaryBase {                                            \
    ALWAYS_INLINE void operate(CBVar &output, const CBVar &input,              \
                               const CBVar &operand) {                         \
      if (input.valueType != operand.valueType)                                \
        throw CBException("Operation not supported between different "         \
                          "types.");                                           \
      switch (input.valueType) {                                               \
      case Int:                                                                \
        if (DIV_BY_ZERO)                                                       \
          if (operand.payload.intValue == 0)                                   \
            throw CBException("Error, division by 0!");                        \
        output.valueType = Int;                                                \
        output.payload.intValue =                                              \
            input.payload.intValue OPERATOR operand.payload.intValue;          \
        break;                                                                 \
      case Int2:                                                               \
        if (DIV_BY_ZERO) {                                                     \
          for (auto i = 0; i < 2; i++)                                         \
            if (operand.payload.int2Value[i] == 0)                             \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Int2;                                               \
        output.payload.int2Value =                                             \
            input.payload.int2Value OPERATOR operand.payload.int2Value;        \
        break;                                                                 \
      case Int3:                                                               \
        if (DIV_BY_ZERO) {                                                     \
          for (auto i = 0; i < 3; i++)                                         \
            if (operand.payload.int3Value[i] == 0)                             \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Int3;                                               \
        output.payload.int3Value =                                             \
            input.payload.int3Value OPERATOR operand.payload.int3Value;        \
        break;                                                                 \
      case Int4:                                                               \
        if (DIV_BY_ZERO) {                                                     \
          for (auto i = 0; i < 4; i++)                                         \
            if (operand.payload.int4Value[i] == 0)                             \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Int4;                                               \
        output.payload.int4Value =                                             \
            input.payload.int4Value OPERATOR operand.payload.int4Value;        \
        break;                                                                 \
      case Int8:                                                               \
        if (DIV_BY_ZERO) {                                                     \
          for (auto i = 0; i < 8; i++)                                         \
            if (operand.payload.int2Value[i] == 0)                             \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Int8;                                               \
        output.payload.int8Value =                                             \
            input.payload.int8Value OPERATOR operand.payload.int8Value;        \
        break;                                                                 \
      case Int16:                                                              \
        if (DIV_BY_ZERO) {                                                     \
          for (auto i = 0; i < 16; i++)                                        \
            if (operand.payload.int2Value[i] == 0)                             \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Int16;                                              \
        output.payload.int16Value =                                            \
            input.payload.int16Value OPERATOR operand.payload.int16Value;      \
        break;                                                                 \
      case Float:                                                              \
        if (DIV_BY_ZERO)                                                       \
          if (operand.payload.floatValue == 0)                                 \
            throw CBException("Error, division by 0!");                        \
        output.valueType = Float;                                              \
        output.payload.floatValue =                                            \
            input.payload.floatValue OPERATOR operand.payload.floatValue;      \
        break;                                                                 \
      case Float2:                                                             \
        if (DIV_BY_ZERO) {                                                     \
          for (auto i = 0; i < 2; i++)                                         \
            if (operand.payload.float2Value[i] == 0)                           \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Float2;                                             \
        output.payload.float2Value =                                           \
            input.payload.float2Value OPERATOR operand.payload.float2Value;    \
        break;                                                                 \
      case Float3:                                                             \
        if (DIV_BY_ZERO) {                                                     \
          for (auto i = 0; i < 3; i++)                                         \
            if (operand.payload.float3Value[i] == 0)                           \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Float3;                                             \
        output.payload.float3Value =                                           \
            input.payload.float3Value OPERATOR operand.payload.float3Value;    \
        break;                                                                 \
      case Float4:                                                             \
        if (DIV_BY_ZERO) {                                                     \
          for (auto i = 0; i < 4; i++)                                         \
            if (operand.payload.float4Value[i] == 0)                           \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Float4;                                             \
        output.payload.float4Value =                                           \
            input.payload.float4Value OPERATOR operand.payload.float4Value;    \
        break;                                                                 \
      case Color:                                                              \
        if (DIV_BY_ZERO) {                                                     \
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
      if (_opType == Normal) {                                                 \
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
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue),             \
                  i < stbds_arrlen(operand.payload.seqValue);                  \
             i++) {                                                            \
          operate(output, input.payload.seqValue[i],                           \
                  operand.payload.seqValue[i]);                                \
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
      if (_opType == Normal) {                                                 \
        operate(output, input, operand);                                       \
        return output;                                                         \
      } else if (_opType == Seq1) {                                            \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {      \
          operate(output, input, operand);                                     \
          stbds_arrpush(_cachedSeq.payload.seqValue, output);                  \
        }                                                                      \
        return _cachedSeq;                                                     \
      } else {                                                                 \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue),             \
                  i < stbds_arrlen(operand.payload.seqValue);                  \
             i++) {                                                            \
          operate(output, input, operand.payload.seqValue[i]);                 \
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
      if (input.valueType == Seq) {                                            \
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
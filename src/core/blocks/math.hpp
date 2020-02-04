/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#pragma once

// TODO, remove most of C macros, use more templates

#include "core.hpp"

namespace chainblocks {
namespace Math {
struct Base {
  static inline Types MathTypes{{
      CoreInfo::IntType,       CoreInfo::IntSeqType,    CoreInfo::Int2Type,
      CoreInfo::Int2SeqType,   CoreInfo::Int3Type,      CoreInfo::Int3SeqType,
      CoreInfo::Int4Type,      CoreInfo::Int4SeqType,   CoreInfo::Int8Type,
      CoreInfo::Int8SeqType,   CoreInfo::Int16Type,     CoreInfo::Int16SeqType,
      CoreInfo::FloatType,     CoreInfo::FloatSeqType,  CoreInfo::Float2Type,
      CoreInfo::Float2SeqType, CoreInfo::Float3Type,    CoreInfo::Float3SeqType,
      CoreInfo::Float4Type,    CoreInfo::Float4SeqType, CoreInfo::ColorType,
      CoreInfo::ColorSeqType,
  }};

  CBVar _cachedSeq{};
  CBVar _output{};

  static CBTypesInfo inputTypes() { return MathTypes; }

  static CBTypesInfo outputTypes() { return MathTypes; }
};

struct UnaryBase : public Base {
  void destroy() { chainblocks::arrayFree(_cachedSeq.payload.seqValue); }

  void setup() { _cachedSeq.valueType = Seq; }
};

struct BinaryBase : public Base {
  enum OpType { Invalid, Normal, Seq1, SeqSeq };

  static inline Types MathTypesOrVar{{
      CoreInfo::IntType,       CoreInfo::IntSeqType,
      CoreInfo::IntVarType,    CoreInfo::IntVarSeqType,
      CoreInfo::Int2Type,      CoreInfo::Int2SeqType,
      CoreInfo::Int2VarType,   CoreInfo::Int2VarSeqType,
      CoreInfo::Int3Type,      CoreInfo::Int3SeqType,
      CoreInfo::Int3VarType,   CoreInfo::Int3VarSeqType,
      CoreInfo::Int4Type,      CoreInfo::Int4SeqType,
      CoreInfo::Int4VarType,   CoreInfo::Int4VarSeqType,
      CoreInfo::Int8Type,      CoreInfo::Int8SeqType,
      CoreInfo::Int8VarType,   CoreInfo::Int8VarSeqType,
      CoreInfo::Int16Type,     CoreInfo::Int16SeqType,
      CoreInfo::Int16VarType,  CoreInfo::Int16VarSeqType,
      CoreInfo::FloatType,     CoreInfo::FloatSeqType,
      CoreInfo::FloatVarType,  CoreInfo::FloatVarSeqType,
      CoreInfo::Float2Type,    CoreInfo::Float2SeqType,
      CoreInfo::Float2VarType, CoreInfo::Float2VarSeqType,
      CoreInfo::Float3Type,    CoreInfo::Float3SeqType,
      CoreInfo::Float3VarType, CoreInfo::Float3VarSeqType,
      CoreInfo::Float4Type,    CoreInfo::Float4SeqType,
      CoreInfo::Float4VarType, CoreInfo::Float4VarSeqType,
      CoreInfo::ColorType,     CoreInfo::ColorSeqType,
      CoreInfo::ColorVarType,  CoreInfo::ColorVarSeqType,
  }};

  static inline ParamsInfo mathParamsInfo =
      ParamsInfo(ParamsInfo::Param("Operand", "The operand.", MathTypesOrVar));

  CBVar _operand{};
  CBVar *_ctxOperand{};
  ExposedInfo _requiredInfo{};
  OpType _opType = Invalid;

  void cleanup() { _ctxOperand = nullptr; }

  void destroy() {
    if (_cachedSeq.valueType == Seq) {
      chainblocks::arrayFree(_cachedSeq.payload.seqValue);
    }
    destroyVar(_operand);
  }

  void setup() {
    _cachedSeq.valueType = Seq;
    _cachedSeq.payload.seqValue = {};
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(mathParamsInfo);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (_operand.valueType == ContextVar) {
      for (uint32_t i = 0; i < data.acquirables.len; i++) {
        if (strcmp(data.acquirables.elements[i].name,
                   _operand.payload.stringValue) == 0) {
          if (data.acquirables.elements[i].exposedType.basicType != Seq &&
              data.inputType.basicType != Seq) {
            _opType = Normal;
          } else if (data.acquirables.elements[i].exposedType.basicType !=
                         Seq &&
                     data.inputType.basicType == Seq) {
            _opType = Seq1;
          } else if (data.acquirables.elements[i].exposedType.basicType ==
                         Seq &&
                     data.inputType.basicType == Seq) {
            _opType = SeqSeq;
          } else {
            throw CBException(
                "Math broadcasting not supported between given types!");
          }
        }
      }
      if (_opType == Invalid) {
        throw CBException("Math operand variable not found: " +
                          std::string(_operand.payload.stringValue));
      }
    } else {
      if (_operand.valueType != Seq && data.inputType.basicType != Seq) {
        _opType = Normal;
      } else if (_operand.valueType != Seq && data.inputType.basicType == Seq) {
        _opType = Seq1;
      } else if (_operand.valueType == Seq && data.inputType.basicType == Seq) {
        _opType = SeqSeq;
      } else {
        throw CBException(
            "Math broadcasting not supported between given types!");
      }
    }
    return data.inputType;
  }

  CBExposedTypesInfo requiredVariables() {
    if (_operand.valueType == ContextVar) {
      _requiredInfo = ExposedInfo(
          ExposedInfo::Variable(_operand.payload.stringValue,
                                "The required operand.", CoreInfo::AnyType));
      return CBExposedTypesInfo(_requiredInfo);
    }
    return {};
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
        if constexpr (DIV_BY_ZERO)                                             \
          if (operand.payload.intValue == 0)                                   \
            throw CBException("Error, division by 0!");                        \
        output.valueType = Int;                                                \
        output.payload.intValue =                                              \
            input.payload.intValue OPERATOR operand.payload.intValue;          \
        break;                                                                 \
      case Int2:                                                               \
        if constexpr (DIV_BY_ZERO) {                                           \
          for (auto i = 0; i < 2; i++)                                         \
            if (operand.payload.int2Value[i] == 0)                             \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Int2;                                               \
        output.payload.int2Value =                                             \
            input.payload.int2Value OPERATOR operand.payload.int2Value;        \
        break;                                                                 \
      case Int3:                                                               \
        if constexpr (DIV_BY_ZERO) {                                           \
          for (auto i = 0; i < 3; i++)                                         \
            if (operand.payload.int3Value[i] == 0)                             \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Int3;                                               \
        output.payload.int3Value =                                             \
            input.payload.int3Value OPERATOR operand.payload.int3Value;        \
        break;                                                                 \
      case Int4:                                                               \
        if constexpr (DIV_BY_ZERO) {                                           \
          for (auto i = 0; i < 4; i++)                                         \
            if (operand.payload.int4Value[i] == 0)                             \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Int4;                                               \
        output.payload.int4Value =                                             \
            input.payload.int4Value OPERATOR operand.payload.int4Value;        \
        break;                                                                 \
      case Int8:                                                               \
        if constexpr (DIV_BY_ZERO) {                                           \
          for (auto i = 0; i < 8; i++)                                         \
            if (operand.payload.int2Value[i] == 0)                             \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Int8;                                               \
        output.payload.int8Value =                                             \
            input.payload.int8Value OPERATOR operand.payload.int8Value;        \
        break;                                                                 \
      case Int16:                                                              \
        if constexpr (DIV_BY_ZERO) {                                           \
          for (auto i = 0; i < 16; i++)                                        \
            if (operand.payload.int2Value[i] == 0)                             \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Int16;                                              \
        output.payload.int16Value =                                            \
            input.payload.int16Value OPERATOR operand.payload.int16Value;      \
        break;                                                                 \
      case Float:                                                              \
        if constexpr (DIV_BY_ZERO)                                             \
          if (operand.payload.floatValue == 0)                                 \
            throw CBException("Error, division by 0!");                        \
        output.valueType = Float;                                              \
        output.payload.floatValue =                                            \
            input.payload.floatValue OPERATOR operand.payload.floatValue;      \
        break;                                                                 \
      case Float2:                                                             \
        if constexpr (DIV_BY_ZERO) {                                           \
          for (auto i = 0; i < 2; i++)                                         \
            if (operand.payload.float2Value[i] == 0)                           \
              throw CBException("Error, division by 0!");                      \
        }                                                                      \
        output.valueType = Float2;                                             \
        output.payload.float2Value =                                           \
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
        _ctxOperand = findVariable(context, _operand.payload.stringValue);     \
      }                                                                        \
      auto &operand = _ctxOperand ? *_ctxOperand : _operand;                   \
      if (likely(_opType == Normal)) {                                         \
        operate(_output, input, operand);                                      \
        return _output;                                                        \
      } else if (_opType == Seq1) {                                            \
        chainblocks::arrayResize(_cachedSeq.payload.seqValue, 0);              \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {            \
          operate(_output, input.payload.seqValue.elements[i], operand);       \
          chainblocks::arrayPush(_cachedSeq.payload.seqValue, _output);        \
        }                                                                      \
        return _cachedSeq;                                                     \
      } else {                                                                 \
        auto olen = operand.payload.seqValue.len;                              \
        chainblocks::arrayResize(_cachedSeq.payload.seqValue, 0);              \
        for (uint32_t i = 0; i < input.payload.seqValue.len && olen > 0;       \
             i++) {                                                            \
          operate(_output, input.payload.seqValue.elements[i],                 \
                  operand.payload.seqValue.elements[i % olen]);                \
          chainblocks::arrayPush(_cachedSeq.payload.seqValue, _output);        \
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
        _ctxOperand = findVariable(context, _operand.payload.stringValue);     \
      }                                                                        \
      auto &operand = _ctxOperand ? *_ctxOperand : _operand;                   \
      if (likely(_opType == Normal)) {                                         \
        operate(_output, input, operand);                                      \
        return _output;                                                        \
      } else if (_opType == Seq1) {                                            \
        chainblocks::arrayResize(_cachedSeq.payload.seqValue, 0);              \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {            \
          operate(_output, input.payload.seqValue.elements[i], operand);       \
          chainblocks::arrayPush(_cachedSeq.payload.seqValue, _output);        \
        }                                                                      \
        return _cachedSeq;                                                     \
      } else {                                                                 \
        auto olen = operand.payload.seqValue.len;                              \
        chainblocks::arrayResize(_cachedSeq.payload.seqValue, 0);              \
        for (uint32_t i = 0; i < input.payload.seqValue.len && olen > 0;       \
             i++) {                                                            \
          operate(_output, input.payload.seqValue.elements[i],                 \
                  operand.payload.seqValue.elements[i % olen]);                \
          chainblocks::arrayPush(_cachedSeq.payload.seqValue, _output);        \
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
  RUNTIME_BLOCK_compose(NAME);                                                 \
  RUNTIME_BLOCK_requiredVariables(NAME);                                       \
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
      if (unlikely(input.valueType == Seq)) {                                  \
        chainblocks::arrayResize(_cachedSeq.payload.seqValue, 0);              \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {            \
          operate(_output, input);                                             \
          chainblocks::arrayPush(_cachedSeq.payload.seqValue, _output);        \
        }                                                                      \
        return _cachedSeq;                                                     \
      } else {                                                                 \
        operate(_output, input);                                               \
        return _output;                                                        \
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
  static CBTypesInfo inputTypes() { return CoreInfo::FloatSeqType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    int64_t inputLen = input.payload.seqValue.len;
    double mean = 0.0;
    auto seq = IterableSeq(input.payload.seqValue);
    for (auto &f : seq) {
      mean += f.payload.floatValue;
    }
    mean /= double(inputLen);
    return Var(mean);
  }
};

template <class T> struct UnaryBin : public T {
  void setOperand(CBType type) {
    switch (type) {
    case CBType::Int:
      T::_operand = Var(1);
      break;
    case CBType::Int2:
      T::_operand = Var(1, 1);
      break;
    case CBType::Int3:
      T::_operand = Var(1, 1, 1);
      break;
    case CBType::Int4:
      T::_operand = Var(1, 1, 1, 1);
      break;
    case CBType::Float:
      T::_operand = Var(1.0);
      break;
    case CBType::Float2:
      T::_operand = Var(1.0, 1.0);
      break;
    case CBType::Float3:
      T::_operand = Var(1.0, 1.0, 1.0);
      break;
    case CBType::Float4:
      T::_operand = Var(1.0, 1.0, 1.0, 1.0);
      break;
    default:
      throw CBException("Type not supported for unary math operation");
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    switch (data.inputType.basicType) {
    case Seq: {
      if (data.inputType.seqTypes.len != 1)
        throw CBException("Expected a Seq with just one type as input");
      setOperand(data.inputType.seqTypes.elements[0].basicType);
    } break;
    default:
      setOperand(data.inputType.basicType);
    }
    return T::compose(data);
  }
};

struct Inc : public UnaryBin<Add> {};
RUNTIME_BLOCK_TYPE(Math, Inc);
struct Dec : public UnaryBin<Subtract> {};
RUNTIME_BLOCK_TYPE(Math, Dec);
}; // namespace Math
}; // namespace chainblocks

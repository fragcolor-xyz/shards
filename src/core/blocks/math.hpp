/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#pragma once

// TODO, remove most of C macros, use more templates

#include "core.hpp"
#include <variant>

namespace chainblocks {
namespace Math {
struct Base {
  static inline Types MathTypes{
      {CoreInfo::IntType, CoreInfo::Int2Type, CoreInfo::Int3Type,
       CoreInfo::Int4Type, CoreInfo::Int8Type, CoreInfo::Int16Type,
       CoreInfo::FloatType, CoreInfo::Float2Type, CoreInfo::Float3Type,
       CoreInfo::Float4Type, CoreInfo::ColorType, CoreInfo::AnySeqType}};

  CBVar _result{};

  void destroy() { destroyVar(_result); }

  CBTypeInfo compose(const CBInstanceData &data) { return data.inputType; }

  static CBTypesInfo inputTypes() { return MathTypes; }

  static CBTypesInfo outputTypes() { return MathTypes; }
};

struct UnaryBase : public Base {};

struct BinaryBase : public Base {
  enum OpType { Invalid, Normal, Seq1, SeqSeq };

  static inline Types MathTypesOrVar{
      {CoreInfo::IntType,    CoreInfo::IntVarType,
       CoreInfo::Int2Type,   CoreInfo::Int2VarType,
       CoreInfo::Int3Type,   CoreInfo::Int3VarType,
       CoreInfo::Int4Type,   CoreInfo::Int4VarType,
       CoreInfo::Int8Type,   CoreInfo::Int8VarType,
       CoreInfo::Int16Type,  CoreInfo::Int16VarType,
       CoreInfo::FloatType,  CoreInfo::FloatVarType,
       CoreInfo::Float2Type, CoreInfo::Float2VarType,
       CoreInfo::Float3Type, CoreInfo::Float3VarType,
       CoreInfo::Float4Type, CoreInfo::Float4VarType,
       CoreInfo::ColorType,  CoreInfo::ColorVarType,
       CoreInfo::AnySeqType, CoreInfo::AnyVarSeqType}};

  static inline ParamsInfo mathParamsInfo = ParamsInfo(
      ParamsInfo::Param("Operand", CBCCSTR("The operand."), MathTypesOrVar));

  ParamVar _operand{Var(0)};
  ExposedInfo _requiredInfo{};
  OpType _opType = Invalid;

  void cleanup() { _operand.cleanup(); }

  void warmup(CBContext *context) { _operand.warmup(context); }

  static CBParametersInfo parameters() {
    return CBParametersInfo(mathParamsInfo);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    CBVar operandSpec = _operand;
    if (operandSpec.valueType == ContextVar) {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        // normal variable
        if (strcmp(data.shared.elements[i].name,
                   operandSpec.payload.stringValue) == 0) {
          if (data.shared.elements[i].exposedType.basicType != Seq &&
              data.inputType.basicType != Seq) {
            if (data.shared.elements[i].exposedType != data.inputType)
              throw ComposeError(
                  "Operation not supported between different types");
            _opType = Normal;
          } else if (data.shared.elements[i].exposedType.basicType != Seq &&
                     data.inputType.basicType == Seq) {
            if (data.inputType.seqTypes.len != 1 ||
                data.shared.elements[i].exposedType !=
                    data.inputType.seqTypes.elements[0])
              throw ComposeError(
                  "Operation not supported between different types");
            _opType = Seq1;
          } else if (data.shared.elements[i].exposedType.basicType == Seq &&
                     data.inputType.basicType == Seq) {
            // TODO need to have deeper types compatibility at least
            _opType = SeqSeq;
          } else {
            throw ComposeError(
                "Math broadcasting not supported between given types!");
          }
        }
      }
      if (_opType == Invalid) {
        throw ComposeError("Math operand variable not found: " +
                           std::string(operandSpec.payload.stringValue));
      }
    } else {
      if (operandSpec.valueType != Seq && data.inputType.basicType != Seq) {
        if (operandSpec.valueType != data.inputType.basicType)
          throw ComposeError("Operation not supported between different types");
        _opType = Normal;
      } else if (operandSpec.valueType != Seq &&
                 data.inputType.basicType == Seq) {
        if (data.inputType.seqTypes.len != 1 ||
            operandSpec.valueType !=
                data.inputType.seqTypes.elements[0].basicType)
          throw ComposeError("Operation not supported between different types");
        _opType = Seq1;
      } else if (operandSpec.valueType == Seq &&
                 data.inputType.basicType == Seq) {
        // TODO need to have deeper types compatibility at least
        _opType = SeqSeq;
      } else {
        throw ComposeError(
            "Math broadcasting not supported between given types!");
      }
    }
    return data.inputType;
  }

  CBExposedTypesInfo requiredVariables() {
    CBVar operandSpec = _operand;
    if (operandSpec.valueType == ContextVar) {
      _requiredInfo = ExposedInfo(ExposedInfo::Variable(
          operandSpec.payload.stringValue, CBCCSTR("The required operand."),
          CoreInfo::AnyType));
      return CBExposedTypesInfo(_requiredInfo);
    }
    return {};
  }

  void setParam(int index, const CBVar &value) { _operand = value; }

  CBVar getParam(int index) { return _operand; }
};

template <class OP> struct BinaryOperation : public BinaryBase {
  void operate(OpType opType, CBVar &output, const CBVar &a, const CBVar &b) {
    if (opType == SeqSeq) {
      if (output.valueType != Seq) {
        destroyVar(output);
        output.valueType = Seq;
      }
      // TODO auto-parallelize with taskflow (should be optional)
      auto olen = b.payload.seqValue.len;
      chainblocks::arrayResize(output.payload.seqValue, 0);
      for (uint32_t i = 0; i < a.payload.seqValue.len && olen > 0; i++) {
        const auto &sa = a.payload.seqValue.elements[i];
        const auto &sb = b.payload.seqValue.elements[i % olen];
        auto type = Normal;
        if (likely(sa.valueType == Seq && sb.valueType == Seq)) {
          type = SeqSeq;
        } else if (sa.valueType == Seq && sb.valueType != Seq) {
          type = Seq1;
        }
        const auto len = output.payload.seqValue.len;
        chainblocks::arrayResize(output.payload.seqValue, len + 1);
        operate(type, output.payload.seqValue.elements[len], sa, sb);
      }
    } else {
      if (opType == Normal && output.valueType == Seq) {
        // something changed, avoid leaking
        // this should happen only here, because compose of SeqSeq is loose
        // we are going from an seq to a regular value, this could be expensive!
        LOG(DEBUG) << "Changing type of output during Math operation, this is "
                      "ok but potentially slow.";
        destroyVar(output);
      }
      operateFast(opType, output, a, b);
    }
  }

  ALWAYS_INLINE void operateFast(OpType opType, CBVar &output, const CBVar &a,
                                 const CBVar &b) {
    OP op;
    if (likely(opType == Normal)) {
      op(output, a, b, this);
    } else if (opType == Seq1) {
      if (output.valueType != Seq) {
        destroyVar(output);
        output.valueType = Seq;
      }
      chainblocks::arrayResize(output.payload.seqValue, 0);
      for (uint32_t i = 0; i < a.payload.seqValue.len; i++) {
        // notice, we use scratch _output here
        CBVar scratch;
        op(scratch, a.payload.seqValue.elements[i], b, this);
        chainblocks::arrayPush(output.payload.seqValue, scratch);
      }
    } else {
      operate(opType, output, a, b);
    }
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    const auto operand = _operand.get();
    operateFast(_opType, _result, input, operand);
    return _result;
  }
};

// TODO implement CBVar operators
// and replace with functional std::plus etc
#define MATH_BINARY_OPERATION(NAME, OPERATOR, DIV_BY_ZERO)                     \
  struct NAME##Op final {                                                      \
    ALWAYS_INLINE void operator()(CBVar &output, const CBVar &input,           \
                                  const CBVar &operand, void *) {              \
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
      case Float:                                                              \
        output.valueType = Float;                                              \
        output.payload.floatValue =                                            \
            input.payload.floatValue OPERATOR operand.payload.floatValue;      \
        break;                                                                 \
      case Float2:                                                             \
        output.valueType = Float2;                                             \
        output.payload.float2Value =                                           \
            input.payload.float2Value OPERATOR operand.payload.float2Value;    \
        break;                                                                 \
      case Float3:                                                             \
        output.valueType = Float3;                                             \
        output.payload.float3Value =                                           \
            input.payload.float3Value OPERATOR operand.payload.float3Value;    \
        break;                                                                 \
      case Float4:                                                             \
        output.valueType = Float4;                                             \
        output.payload.float4Value =                                           \
            input.payload.float4Value OPERATOR operand.payload.float4Value;    \
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
        throw ActivationError(                                                 \
            #NAME " operation not supported between given types!");            \
      }                                                                        \
    }                                                                          \
  };                                                                           \
  using NAME = BinaryOperation<NAME##Op>;                                      \
  RUNTIME_BLOCK_TYPE(Math, NAME);

#define MATH_BINARY_INT_OPERATION(NAME, OPERATOR)                              \
  struct NAME##Op {                                                            \
    ALWAYS_INLINE void operator()(CBVar &output, const CBVar &input,           \
                                  const CBVar &operand, void *) {              \
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
        throw ActivationError(                                                 \
            #NAME " operation not supported between given types!");            \
      }                                                                        \
    }                                                                          \
  };                                                                           \
  using NAME = BinaryOperation<NAME##Op>;                                      \
  RUNTIME_BLOCK_TYPE(Math, NAME);

MATH_BINARY_OPERATION(Add, +, 0);
MATH_BINARY_OPERATION(Subtract, -, 0);
MATH_BINARY_OPERATION(Multiply, *, 0);
MATH_BINARY_OPERATION(Divide, /, 1);
MATH_BINARY_INT_OPERATION(Xor, ^);
MATH_BINARY_INT_OPERATION(And, &);
MATH_BINARY_INT_OPERATION(Or, |);
MATH_BINARY_INT_OPERATION(Mod, %);
MATH_BINARY_INT_OPERATION(LShift, <<);
MATH_BINARY_INT_OPERATION(RShift, >>);

// Not used for now...
#define MATH_UNARY_FUNCTOR(NAME, FUNCD, FUNCF)                                 \
  struct NAME##UnaryFuncD {                                                    \
    ALWAYS_INLINE double operator()(double x) { return FUNCD(x); }             \
  };                                                                           \
  struct NAME##UnaryFuncF {                                                    \
    ALWAYS_INLINE float operator()(float x) { return FUNCF(x); }               \
  };

MATH_UNARY_FUNCTOR(Abs, __builtin_fabs, __builtin_fabsf);
MATH_UNARY_FUNCTOR(Exp, __builtin_exp, __builtin_expf);
MATH_UNARY_FUNCTOR(Exp2, __builtin_exp2, __builtin_exp2f);
MATH_UNARY_FUNCTOR(Expm1, __builtin_expm1, __builtin_expm1f);
MATH_UNARY_FUNCTOR(Log, __builtin_log, __builtin_logf);
MATH_UNARY_FUNCTOR(Log10, __builtin_log10, __builtin_log10f);
MATH_UNARY_FUNCTOR(Log2, __builtin_log2, __builtin_log2f);
MATH_UNARY_FUNCTOR(Log1p, __builtin_log1p, __builtin_log1pf);
MATH_UNARY_FUNCTOR(Sqrt, __builtin_sqrt, __builtin_sqrtf);
MATH_UNARY_FUNCTOR(Cbrt, __builtin_cbrt, __builtin_cbrt);
MATH_UNARY_FUNCTOR(Sin, __builtin_sin, __builtin_sinf);
MATH_UNARY_FUNCTOR(Cos, __builtin_cos, __builtin_cosf);
MATH_UNARY_FUNCTOR(Tan, __builtin_tan, __builtin_tanf);
MATH_UNARY_FUNCTOR(Asin, __builtin_asin, __builtin_asinf);
MATH_UNARY_FUNCTOR(Acos, __builtin_acos, __builtin_acosf);
MATH_UNARY_FUNCTOR(Atan, __builtin_atan, __builtin_atanf);
MATH_UNARY_FUNCTOR(Sinh, __builtin_sinh, __builtin_sinhf);
MATH_UNARY_FUNCTOR(Cosh, __builtin_cosh, __builtin_coshf);
MATH_UNARY_FUNCTOR(Tanh, __builtin_tanh, __builtin_tanhf);
MATH_UNARY_FUNCTOR(Asinh, __builtin_asinh, __builtin_asinhf);
MATH_UNARY_FUNCTOR(Acosh, __builtin_acosh, __builtin_acoshf);
MATH_UNARY_FUNCTOR(Atanh, __builtin_atanh, __builtin_atanhf);
MATH_UNARY_FUNCTOR(Erf, __builtin_erf, __builtin_erff);
MATH_UNARY_FUNCTOR(Erfc, __builtin_erfc, __builtin_erfcf);
MATH_UNARY_FUNCTOR(TGamma, __builtin_tgamma, __builtin_tgammaf);
MATH_UNARY_FUNCTOR(LGamma, __builtin_lgamma, __builtin_lgammaf);
MATH_UNARY_FUNCTOR(Ceil, __builtin_ceil, __builtin_ceilf);
MATH_UNARY_FUNCTOR(Floor, __builtin_floor, __builtin_floorf);
MATH_UNARY_FUNCTOR(Trunc, __builtin_trunc, __builtin_truncf);
MATH_UNARY_FUNCTOR(Round, __builtin_round, __builtin_roundf);

template <CBType CBT, typename FuncD, typename FuncF> struct UnaryOperation {
  ALWAYS_INLINE void operate(CBVar &output, const CBVar &input) {
    FuncD fd;
    FuncF ff;
    if constexpr (CBT == CBType::Float) {
      output.valueType = Float;
      output.payload.floatValue = fd(input.payload.floatValue);
    } else if constexpr (CBT == CBType::Float2) {
      output.valueType = Float2;
      output.payload.float2Value[0] = fd(input.payload.float2Value[0]);
      output.payload.float2Value[1] = fd(input.payload.float2Value[1]);
    } else if constexpr (CBT == CBType::Float3) {
      output.valueType = Float3;
      output.payload.float3Value[0] = ff(input.payload.float3Value[0]);
      output.payload.float3Value[1] = ff(input.payload.float3Value[1]);
      output.payload.float3Value[2] = ff(input.payload.float3Value[2]);
    } else if constexpr (CBT == CBType::Float4) {
      output.valueType = Float4;
      output.payload.float4Value[0] = ff(input.payload.float4Value[0]);
      output.payload.float4Value[1] = ff(input.payload.float4Value[1]);
      output.payload.float4Value[2] = ff(input.payload.float4Value[2]);
      output.payload.float4Value[3] = ff(input.payload.float4Value[3]);
    }
  }
};
// End of not used for now

#define MATH_UNARY_OPERATION(NAME, FUNC, FUNCF)                                \
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
        throw ActivationError(                                                 \
            #NAME " operation not supported between given types!");            \
      }                                                                        \
    }                                                                          \
                                                                               \
    ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {     \
      if (unlikely(input.valueType == Seq)) {                                  \
        _result.valueType = Seq;                                               \
        chainblocks::arrayResize(_result.payload.seqValue, 0);                 \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {            \
          CBVar scratch;                                                       \
          operate(scratch, input.payload.seqValue.elements[i]);                \
          chainblocks::arrayPush(_result.payload.seqValue, scratch);           \
        }                                                                      \
        return _result;                                                        \
      } else {                                                                 \
        CBVar scratch;                                                         \
        operate(scratch, input);                                               \
        return scratch;                                                        \
      }                                                                        \
    }                                                                          \
  };                                                                           \
  RUNTIME_BLOCK_TYPE(Math, NAME);

MATH_UNARY_OPERATION(Abs, __builtin_fabs, __builtin_fabsf);
MATH_UNARY_OPERATION(Exp, __builtin_exp, __builtin_expf);
MATH_UNARY_OPERATION(Exp2, __builtin_exp2, __builtin_exp2f);
MATH_UNARY_OPERATION(Expm1, __builtin_expm1, __builtin_expm1f);
MATH_UNARY_OPERATION(Log, __builtin_log, __builtin_logf);
MATH_UNARY_OPERATION(Log10, __builtin_log10, __builtin_log10f);
MATH_UNARY_OPERATION(Log2, __builtin_log2, __builtin_log2f);
MATH_UNARY_OPERATION(Log1p, __builtin_log1p, __builtin_log1pf);
MATH_UNARY_OPERATION(Sqrt, __builtin_sqrt, __builtin_sqrtf);
MATH_UNARY_OPERATION(Cbrt, __builtin_cbrt, __builtin_cbrt);
MATH_UNARY_OPERATION(Sin, __builtin_sin, __builtin_sinf);
MATH_UNARY_OPERATION(Cos, __builtin_cos, __builtin_cosf);
MATH_UNARY_OPERATION(Tan, __builtin_tan, __builtin_tanf);
MATH_UNARY_OPERATION(Asin, __builtin_asin, __builtin_asinf);
MATH_UNARY_OPERATION(Acos, __builtin_acos, __builtin_acosf);
MATH_UNARY_OPERATION(Atan, __builtin_atan, __builtin_atanf);
MATH_UNARY_OPERATION(Sinh, __builtin_sinh, __builtin_sinhf);
MATH_UNARY_OPERATION(Cosh, __builtin_cosh, __builtin_coshf);
MATH_UNARY_OPERATION(Tanh, __builtin_tanh, __builtin_tanhf);
MATH_UNARY_OPERATION(Asinh, __builtin_asinh, __builtin_asinhf);
MATH_UNARY_OPERATION(Acosh, __builtin_acosh, __builtin_acoshf);
MATH_UNARY_OPERATION(Atanh, __builtin_atanh, __builtin_atanhf);
MATH_UNARY_OPERATION(Erf, __builtin_erf, __builtin_erff);
MATH_UNARY_OPERATION(Erfc, __builtin_erfc, __builtin_erfcf);
MATH_UNARY_OPERATION(TGamma, __builtin_tgamma, __builtin_tgammaf);
MATH_UNARY_OPERATION(LGamma, __builtin_lgamma, __builtin_lgammaf);
MATH_UNARY_OPERATION(Ceil, __builtin_ceil, __builtin_ceilf);
MATH_UNARY_OPERATION(Floor, __builtin_floor, __builtin_floorf);
MATH_UNARY_OPERATION(Trunc, __builtin_trunc, __builtin_truncf);
MATH_UNARY_OPERATION(Round, __builtin_round, __builtin_roundf);

struct Mean {
  struct ArithMean {
    double operator()(const CBSeq &seq) {
      const uint32_t inputLen = seq.len;
      double mean = 0.0;
      for (uint32_t i = 0; i < inputLen; i++) {
        const auto &v = seq.elements[i];
        mean += v.payload.floatValue;
      }
      mean /= double(inputLen);
      return mean;
    }
  };

  struct GeoMean {
    double operator()(const CBSeq &seq) {
      const uint32_t inputLen = seq.len;
      double mean = 1.0;
      for (uint32_t i = 0; i < inputLen; i++) {
        const auto &v = seq.elements[i];
        mean *= v.payload.floatValue;
      }
      return std::pow(mean, 1.0 / double(inputLen));
    }
  };

  struct HarmoMean {
    double operator()(const CBSeq &seq) {
      const uint32_t inputLen = seq.len;
      double mean = 0.0;
      for (uint32_t i = 0; i < inputLen; i++) {
        const auto &v = seq.elements[i];
        mean += 1.0 / v.payload.floatValue;
      }
      return double(inputLen) / mean;
    }
  };

  enum class MeanKind { Arithmetic, Geometric, Harmonic };
  static inline EnumInfo<MeanKind> _meanEnum{"Mean", CoreCC, 'mean'};

  static CBTypesInfo inputTypes() { return CoreInfo::FloatSeqType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static CBParametersInfo parameters() {
    static Type kind{{CBType::Enum, {.enumeration = {CoreCC, 'mean'}}}};
    static Parameters params{
        {"Kind", CBCCSTR("The kind of Pythagorean means."), {kind}}};
    return params;
  }

  void setParam(int index, const CBVar &value) {
    mean = MeanKind(value.payload.enumValue);
  }

  CBVar getParam(int index) { return Var::Enum(mean, CoreCC, 'mean'); }

  CBVar activate(CBContext *context, const CBVar &input) {
    switch (mean) {
    case MeanKind::Arithmetic: {
      ArithMean m;
      return Var(m(input.payload.seqValue));
    }
    case MeanKind::Geometric: {
      GeoMean m;
      return Var(m(input.payload.seqValue));
    }
    case MeanKind::Harmonic: {
      HarmoMean m;
      return Var(m(input.payload.seqValue));
    }
    default:
      throw ActivationError("Invalid mean case.");
    }
  }

  MeanKind mean{MeanKind::Arithmetic};
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
      throw ActivationError("Type not supported for unary math operation");
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    switch (data.inputType.basicType) {
    case Seq: {
      if (data.inputType.seqTypes.len != 1)
        throw ComposeError("Expected a Seq with just one type as input");
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

struct MaxOp final {
  ALWAYS_INLINE void operator()(CBVar &output, const CBVar &input,
                                const CBVar &operand, void *) {
    output = std::max(input, operand);
  }
};
using Max = BinaryOperation<MaxOp>;

struct MinOp final {
  ALWAYS_INLINE void operator()(CBVar &output, const CBVar &input,
                                const CBVar &operand, void *) {
    output = std::min(input, operand);
  }
};
using Min = BinaryOperation<MinOp>;

#define MATH_BINARY_FLOAT_PROC(NAME, PROC)                                     \
  struct NAME##Op final {                                                      \
    ALWAYS_INLINE void operator()(CBVar &output, const CBVar &input,           \
                                  const CBVar &operand, void *) {              \
      switch (input.valueType) {                                               \
      case Float:                                                              \
        output.valueType = Float;                                              \
        output.payload.floatValue =                                            \
            PROC(input.payload.floatValue, operand.payload.floatValue);        \
        break;                                                                 \
      case Float2:                                                             \
        output.valueType = Float2;                                             \
        output.payload.float2Value[0] = PROC(input.payload.float2Value[0],     \
                                             operand.payload.float2Value[0]);  \
        output.payload.float2Value[1] = PROC(input.payload.float2Value[1],     \
                                             operand.payload.float2Value[1]);  \
        break;                                                                 \
      case Float3:                                                             \
        output.valueType = Float3;                                             \
        output.payload.float3Value[0] = PROC(input.payload.float3Value[0],     \
                                             operand.payload.float3Value[0]);  \
        output.payload.float3Value[1] = PROC(input.payload.float3Value[1],     \
                                             operand.payload.float3Value[1]);  \
        output.payload.float3Value[2] = PROC(input.payload.float3Value[2],     \
                                             operand.payload.float3Value[2]);  \
        break;                                                                 \
      case Float4:                                                             \
        output.valueType = Float4;                                             \
        output.payload.float4Value[0] = PROC(input.payload.float4Value[0],     \
                                             operand.payload.float4Value[0]);  \
        output.payload.float4Value[1] = PROC(input.payload.float4Value[1],     \
                                             operand.payload.float4Value[1]);  \
        output.payload.float4Value[2] = PROC(input.payload.float4Value[2],     \
                                             operand.payload.float4Value[2]);  \
        output.payload.float4Value[3] = PROC(input.payload.float4Value[3],     \
                                             operand.payload.float4Value[3]);  \
        break;                                                                 \
      default:                                                                 \
        throw ActivationError(                                                 \
            #NAME " operation not supported between given types!");            \
      }                                                                        \
    }                                                                          \
  };                                                                           \
  using NAME = BinaryOperation<NAME##Op>;

MATH_BINARY_FLOAT_PROC(Pow, std::pow);
using Pow = BinaryOperation<PowOp>;

inline void registerBlocks() {
  REGISTER_CBLOCK("Math.Add", Add);
  REGISTER_CBLOCK("Math.Subtract", Subtract);
  REGISTER_CBLOCK("Math.Multiply", Multiply);
  REGISTER_CBLOCK("Math.Divide", Divide);
  REGISTER_CBLOCK("Math.Xor", Xor);
  REGISTER_CBLOCK("Math.And", And);
  REGISTER_CBLOCK("Math.Or", Or);
  REGISTER_CBLOCK("Math.Mod", Mod);
  REGISTER_CBLOCK("Math.LShift", LShift);
  REGISTER_CBLOCK("Math.RShift", RShift);

  REGISTER_CBLOCK("Math.Abs", Abs);
  REGISTER_CBLOCK("Math.Exp", Exp);
  REGISTER_CBLOCK("Math.Exp2", Exp2);
  REGISTER_CBLOCK("Math.Expm1", Expm1);
  REGISTER_CBLOCK("Math.Log", Log);
  REGISTER_CBLOCK("Math.Log10", Log10);
  REGISTER_CBLOCK("Math.Log2", Log2);
  REGISTER_CBLOCK("Math.Log1p", Log1p);
  REGISTER_CBLOCK("Math.Sqrt", Sqrt);
  REGISTER_CBLOCK("Math.Cbrt", Cbrt);
  REGISTER_CBLOCK("Math.Sin", Sin);
  REGISTER_CBLOCK("Math.Cos", Cos);
  REGISTER_CBLOCK("Math.Tan", Tan);
  REGISTER_CBLOCK("Math.Asin", Asin);
  REGISTER_CBLOCK("Math.Acos", Acos);
  REGISTER_CBLOCK("Math.Atan", Atan);
  REGISTER_CBLOCK("Math.Sinh", Sinh);
  REGISTER_CBLOCK("Math.Cosh", Cosh);
  REGISTER_CBLOCK("Math.Tanh", Tanh);
  REGISTER_CBLOCK("Math.Asinh", Asinh);
  REGISTER_CBLOCK("Math.Acosh", Acosh);
  REGISTER_CBLOCK("Math.Atanh", Atanh);
  REGISTER_CBLOCK("Math.Erf", Erf);
  REGISTER_CBLOCK("Math.Erfc", Erfc);
  REGISTER_CBLOCK("Math.TGamma", TGamma);
  REGISTER_CBLOCK("Math.LGamma", LGamma);
  REGISTER_CBLOCK("Math.Ceil", Ceil);
  REGISTER_CBLOCK("Math.Floor", Floor);
  REGISTER_CBLOCK("Math.Trunc", Trunc);
  REGISTER_CBLOCK("Math.Round", Round);

  REGISTER_CBLOCK("Math.Mean", Mean);
  REGISTER_CBLOCK("Math.Inc", Inc);
  REGISTER_CBLOCK("Math.Dec", Dec);

  REGISTER_CBLOCK("Max", Max);
  REGISTER_CBLOCK("Min", Min);
  REGISTER_CBLOCK("Math.Pow", Pow);
}

}; // namespace Math
}; // namespace chainblocks

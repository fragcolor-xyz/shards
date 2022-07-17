/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

// TODO, remove most of C macros, use more templates

#ifndef SH_CORE_SHARDS_MATH
#define SH_CORE_SHARDS_MATH

#include "shards.h"
#include "shards.hpp"
#include "core.hpp"
#include <sstream>
#include <stdexcept>
#include <variant>

#define _PC
#include "ShaderFastMathLib.h"
#undef _PC

namespace shards {
namespace Math {
struct Base {
  static inline Types MathTypes{{CoreInfo::IntType, CoreInfo::Int2Type, CoreInfo::Int3Type, CoreInfo::Int4Type,
                                 CoreInfo::Int8Type, CoreInfo::Int16Type, CoreInfo::FloatType, CoreInfo::Float2Type,
                                 CoreInfo::Float3Type, CoreInfo::Float4Type, CoreInfo::ColorType, CoreInfo::AnySeqType}};

  SHVar _result{};

  void destroy() { destroyVar(_result); }

  SHTypeInfo compose(const SHInstanceData &data) { return data.inputType; }

  static SHTypesInfo inputTypes() { return MathTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Any valid integer(s), floating point number(s), or a sequence of such entities supported by this operation.");
  }

  static SHTypesInfo outputTypes() { return MathTypes; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("The result of the operation, usually in the same type as "
                   "the input value.");
  }
};

struct UnaryBase : public Base {
  static inline Types FloatOrSeqTypes{
      {CoreInfo::FloatType, CoreInfo::Float2Type, CoreInfo::Float3Type, CoreInfo::Float4Type, CoreInfo::AnySeqType}};

  static SHTypesInfo inputTypes() { return FloatOrSeqTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Any valid floating point number(s) or a sequence of such entities supported by this operation.");
  }
  static SHTypesInfo outputTypes() { return FloatOrSeqTypes; }
};

struct BinaryBase : public Base {
  enum OpType { Invalid, Broadcast, Normal, Seq1, SeqSeq };

  static inline Types MathTypesOrVar{
      {CoreInfo::IntType,       CoreInfo::IntVarType,   CoreInfo::Int2Type,      CoreInfo::Int2VarType,  CoreInfo::Int3Type,
       CoreInfo::Int3VarType,   CoreInfo::Int4Type,     CoreInfo::Int4VarType,   CoreInfo::Int8Type,     CoreInfo::Int8VarType,
       CoreInfo::Int16Type,     CoreInfo::Int16VarType, CoreInfo::FloatType,     CoreInfo::FloatVarType, CoreInfo::Float2Type,
       CoreInfo::Float2VarType, CoreInfo::Float3Type,   CoreInfo::Float3VarType, CoreInfo::Float4Type,   CoreInfo::Float4VarType,
       CoreInfo::ColorType,     CoreInfo::ColorVarType, CoreInfo::AnySeqType,    CoreInfo::AnyVarSeqType}};

  static inline ParamsInfo mathParamsInfo =
      ParamsInfo(ParamsInfo::Param("Operand", SHCCSTR("The operand for this operation."), MathTypesOrVar));

  ParamVar _operand{shards::Var(0)};
  ExposedInfo _requiredInfo{};
  OpType _opType = Invalid;
  const VectorTypeTraits *_lhsVecType{};
  const VectorTypeTraits *_rhsVecType{};

  void cleanup() { _operand.cleanup(); }

  void warmup(SHContext *context) { _operand.warmup(context); }

  static SHParametersInfo parameters() { return SHParametersInfo(mathParamsInfo); }

  ComposeError formatTypeError(const SHType &inputType, const SHType &paramType) {
    std::stringstream errStream;
    errStream << "Operation not supported between different types ";
    errStream << "(input=" << type2Name(inputType);
    errStream << ", param=" << type2Name(paramType) << ")";
    return ComposeError(errStream.str());
  }

  void validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    if (rhs != Seq && lhs.basicType != Seq) {
      _lhsVecType = VectorTypeLookup::getInstance().get(lhs.basicType);
      _rhsVecType = VectorTypeLookup::getInstance().get(rhs);
      if (_lhsVecType || _rhsVecType) {
        if (!_lhsVecType || !_rhsVecType)
          throw ComposeError("Unsupported type to Math.Multiply");

        bool sameDimension = _lhsVecType->dimension == _rhsVecType->dimension;
        // Don't check number type here because we have to convert Float64 -> Float32 for broadcasting
        if (!_lhsVecType->isInteger && !sameDimension && (_lhsVecType->dimension == 1 || _rhsVecType->dimension == 1)) {
          _opType = Broadcast;
          // Result is the vector type
          if (_rhsVecType->dimension == 1) {
            resultType = _lhsVecType->type;
          } else {
            resultType = _rhsVecType->type;
          }
        } else {
          if (!sameDimension || _lhsVecType->numberType != _rhsVecType->numberType) {
            throw ComposeError(
                fmt::format("Can not multiply vector of size {} and {}", _lhsVecType->dimension, _rhsVecType->dimension));
          }
          _opType = Normal;
        }
      }
    } else if (rhs != Seq && lhs.basicType == Seq) {
      if (lhs.seqTypes.len != 1 || rhs != lhs.seqTypes.elements[0].basicType)
        throw formatTypeError(lhs.seqTypes.elements[0].basicType, rhs);
      _opType = Seq1;
    } else if (rhs == Seq && lhs.basicType == Seq) {
      // TODO need to have deeper types compatibility at least
      _opType = SeqSeq;
    } else {
      throw ComposeError("Math broadcasting not supported between given types!");
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    SHTypeInfo resultType = data.inputType;
    SHVar operandSpec = _operand;
    if (operandSpec.valueType == ContextVar) {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        // normal variable
        if (strcmp(data.shared.elements[i].name, operandSpec.payload.stringValue) == 0) {
          validateTypes(data.inputType, data.shared.elements[i].exposedType.basicType, resultType);
          break;
        }
      }
    } else {
      validateTypes(data.inputType, operandSpec.valueType, resultType);
    }

    if (_opType == Invalid) {
      throw ComposeError("Math operand variable not found: " + std::string(operandSpec.payload.stringValue));
    }

    return resultType;
  }

  SHExposedTypesInfo requiredVariables() {
    SHVar operandSpec = _operand;
    if (operandSpec.valueType == ContextVar) {
      _requiredInfo = ExposedInfo(
          ExposedInfo::Variable(operandSpec.payload.stringValue, SHCCSTR("The required operand."), CoreInfo::AnyType));
      return SHExposedTypesInfo(_requiredInfo);
    }
    return {};
  }

  void setParam(int index, const SHVar &value) { _operand = value; }

  SHVar getParam(int index) { return _operand; }
};

template <class OP> struct BinaryOperation : public BinaryBase {
  SH_HAS_MEMBER_TEST(hasApply);

  static SHOptionalString help() {
    return SHCCSTR("Applies the binary operation on the input value and the operand and returns the result (or a sequence of "
                   "results if the input and the operand are sequences).");
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    SHTypeInfo resultType = BinaryBase::compose(data);
    if (_opType == Broadcast) {
      if constexpr (!has_hasApply<OP>::value) {
        throw ComposeError("Operator broadcast not supported for this type");
      }
    }
    return resultType;
  }

  void operate(OpType opType, SHVar &output, const SHVar &a, const SHVar &b) {
    if (opType == Broadcast) {
      // This implements broadcast operators on float types
      SHVar scalar = a;
      SHVar vec = b;
      const VectorTypeTraits *scalarType = _lhsVecType;
      const VectorTypeTraits *vecType = _rhsVecType;
      bool swapped = false;
      if (_rhsVecType->dimension == 1) {
        std::swap(scalar, vec);
        std::swap(scalarType, vecType);
        swapped = true;
      }

      if constexpr (has_hasApply<OP>::value) {
        OP op;
        output = vec;
        if (vecType->numberType != scalarType->numberType) {
          // Should be only one case for floating point types
          assert(vecType->numberType == NumberType::Float32);
          assert(scalarType->numberType == NumberType::Float64);

          if (swapped) {
            for (size_t i = 0; i < vecType->dimension; i++) {
              output.payload.float4Value[i] =
                  float(op.template apply<SHFloat>(SHFloat(output.payload.float4Value[i]), scalar.payload.floatValue));
            }
          } else {
            for (size_t i = 0; i < vecType->dimension; i++) {
              output.payload.float4Value[i] =
                  float(op.template apply<SHFloat>(scalar.payload.floatValue, SHFloat(output.payload.float4Value[i])));
            }
          }
        } else {
          if (swapped) {
            for (size_t i = 0; i < vecType->dimension; i++) {
              output.payload.float2Value[i] =
                  op.template apply<SHFloat>(output.payload.float2Value[i], scalar.payload.floatValue);
            }
          } else {
            for (size_t i = 0; i < vecType->dimension; i++) {
              output.payload.float2Value[i] =
                  op.template apply<SHFloat>(scalar.payload.floatValue, output.payload.float2Value[i]);
            }
          }
        }
      } else {
        // This should be caught during compose
        throw std::logic_error("Operator broadcast not supported for this type");
      }
    } else if (opType == SeqSeq) {
      if (output.valueType != Seq) {
        destroyVar(output);
        output.valueType = Seq;
      }
      // TODO auto-parallelize with taskflow (should be optional)
      auto olen = b.payload.seqValue.len;
      shards::arrayResize(output.payload.seqValue, 0);
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
        shards::arrayResize(output.payload.seqValue, len + 1);
        operate(type, output.payload.seqValue.elements[len], sa, sb);
      }
    } else {
      if (opType == Normal && output.valueType == Seq) {
        // something changed, avoid leaking
        // this should happen only here, because compose of SeqSeq is loose
        // we are going from an seq to a regular value, this could be expensive!
        SHLOG_DEBUG("Changing type of output during Math operation, this is ok "
                    "but potentially slow");
        destroyVar(output);
      }
      operateFast(opType, output, a, b);
    }
  }

  ALWAYS_INLINE void operateFast(OpType opType, SHVar &output, const SHVar &a, const SHVar &b) {
    OP op;
    if (likely(opType == Normal)) {
      op(output, a, b, this);
    } else if (opType == Seq1) {
      if (output.valueType != Seq) {
        destroyVar(output);
        output.valueType = Seq;
      }
      shards::arrayResize(output.payload.seqValue, 0);
      for (uint32_t i = 0; i < a.payload.seqValue.len; i++) {
        // notice, we use scratch _output here
        SHVar scratch;
        op(scratch, a.payload.seqValue.elements[i], b, this);
        shards::arrayPush(output.payload.seqValue, scratch);
      }
    } else {
      operate(opType, output, a, b);
    }
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    const auto operand = _operand.get();
    operateFast(_opType, _result, input, operand);
    return _result;
  }
};

template <class OP> struct BinaryIntOperation : public BinaryOperation<OP> {
  static inline Types IntOrSeqTypes{{CoreInfo::IntType, CoreInfo::Int2Type, CoreInfo::Int3Type, CoreInfo::Int4Type,
                                     CoreInfo::Int8Type, CoreInfo::Int16Type, CoreInfo::ColorType, CoreInfo::AnySeqType}};

  static SHTypesInfo inputTypes() { return IntOrSeqTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Any valid integer(s) or a sequence of such entities supported by this operation.");
  }
  static SHTypesInfo outputTypes() { return IntOrSeqTypes; }
};

// TODO implement SHVar operators
// and replace with functional std::plus etc
#define MATH_BINARY_OPERATION(NAME, OPERATOR, DIV_BY_ZERO)                                              \
  struct NAME##Op final {                                                                               \
    static constexpr bool hasApply{};                                                                   \
    template <typename T> T apply(const T &lhs, const T &rhs) { return lhs OPERATOR rhs; }              \
    ALWAYS_INLINE void operator()(SHVar &output, const SHVar &input, const SHVar &operand, void *) {    \
      switch (input.valueType) {                                                                        \
      case Int:                                                                                         \
        output.valueType = Int;                                                                         \
        output.payload.intValue = input.payload.intValue OPERATOR operand.payload.intValue;             \
        break;                                                                                          \
      case Int2:                                                                                        \
        output.valueType = Int2;                                                                        \
        output.payload.int2Value = input.payload.int2Value OPERATOR operand.payload.int2Value;          \
        break;                                                                                          \
      case Int3:                                                                                        \
        output.valueType = Int3;                                                                        \
        output.payload.int3Value = input.payload.int3Value OPERATOR operand.payload.int3Value;          \
        break;                                                                                          \
      case Int4:                                                                                        \
        output.valueType = Int4;                                                                        \
        output.payload.int4Value = input.payload.int4Value OPERATOR operand.payload.int4Value;          \
        break;                                                                                          \
      case Int8:                                                                                        \
        output.valueType = Int8;                                                                        \
        output.payload.int8Value = input.payload.int8Value OPERATOR operand.payload.int8Value;          \
        break;                                                                                          \
      case Int16:                                                                                       \
        output.valueType = Int16;                                                                       \
        output.payload.int16Value = input.payload.int16Value OPERATOR operand.payload.int16Value;       \
        break;                                                                                          \
      case Float:                                                                                       \
        output.valueType = Float;                                                                       \
        output.payload.floatValue = input.payload.floatValue OPERATOR operand.payload.floatValue;       \
        break;                                                                                          \
      case Float2:                                                                                      \
        output.valueType = Float2;                                                                      \
        output.payload.float2Value = input.payload.float2Value OPERATOR operand.payload.float2Value;    \
        break;                                                                                          \
      case Float3:                                                                                      \
        output.valueType = Float3;                                                                      \
        output.payload.float3Value = input.payload.float3Value OPERATOR operand.payload.float3Value;    \
        break;                                                                                          \
      case Float4:                                                                                      \
        output.valueType = Float4;                                                                      \
        output.payload.float4Value = input.payload.float4Value OPERATOR operand.payload.float4Value;    \
        break;                                                                                          \
      case Color:                                                                                       \
        output.valueType = Color;                                                                       \
        output.payload.colorValue.r = input.payload.colorValue.r OPERATOR operand.payload.colorValue.r; \
        output.payload.colorValue.g = input.payload.colorValue.g OPERATOR operand.payload.colorValue.g; \
        output.payload.colorValue.b = input.payload.colorValue.b OPERATOR operand.payload.colorValue.b; \
        output.payload.colorValue.a = input.payload.colorValue.a OPERATOR operand.payload.colorValue.a; \
        break;                                                                                          \
      default:                                                                                          \
        throw ActivationError(#NAME " operation not supported between given types!");                   \
      }                                                                                                 \
    }                                                                                                   \
  };                                                                                                    \
  using NAME = BinaryOperation<NAME##Op>;                                                               \
  RUNTIME_SHARD_TYPE(Math, NAME);

#define MATH_BINARY_INT_OPERATION(NAME, OPERATOR)                                                       \
  struct NAME##Op final {                                                                               \
    ALWAYS_INLINE void operator()(SHVar &output, const SHVar &input, const SHVar &operand, void *) {    \
      switch (input.valueType) {                                                                        \
      case Int:                                                                                         \
        output.valueType = Int;                                                                         \
        output.payload.intValue = input.payload.intValue OPERATOR operand.payload.intValue;             \
        break;                                                                                          \
      case Int2:                                                                                        \
        output.valueType = Int2;                                                                        \
        output.payload.int2Value = input.payload.int2Value OPERATOR operand.payload.int2Value;          \
        break;                                                                                          \
      case Int3:                                                                                        \
        output.valueType = Int3;                                                                        \
        output.payload.int3Value = input.payload.int3Value OPERATOR operand.payload.int3Value;          \
        break;                                                                                          \
      case Int4:                                                                                        \
        output.valueType = Int4;                                                                        \
        output.payload.int4Value = input.payload.int4Value OPERATOR operand.payload.int4Value;          \
        break;                                                                                          \
      case Int8:                                                                                        \
        output.valueType = Int8;                                                                        \
        output.payload.int8Value = input.payload.int8Value OPERATOR operand.payload.int8Value;          \
        break;                                                                                          \
      case Int16:                                                                                       \
        output.valueType = Int16;                                                                       \
        output.payload.int16Value = input.payload.int16Value OPERATOR operand.payload.int16Value;       \
        break;                                                                                          \
      case Color:                                                                                       \
        output.valueType = Color;                                                                       \
        output.payload.colorValue.r = input.payload.colorValue.r OPERATOR operand.payload.colorValue.r; \
        output.payload.colorValue.g = input.payload.colorValue.g OPERATOR operand.payload.colorValue.g; \
        output.payload.colorValue.b = input.payload.colorValue.b OPERATOR operand.payload.colorValue.b; \
        output.payload.colorValue.a = input.payload.colorValue.a OPERATOR operand.payload.colorValue.a; \
        break;                                                                                          \
      default:                                                                                          \
        throw ActivationError(#NAME " operation not supported between given types!");                   \
      }                                                                                                 \
    }                                                                                                   \
  };                                                                                                    \
  using NAME = BinaryIntOperation<NAME##Op>;                                                            \
  RUNTIME_SHARD_TYPE(Math, NAME);

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

#if 0
// Not used for now...

#define MATH_UNARY_FUNCTOR(NAME, FUNCD, FUNCF)                     \
  struct NAME##UnaryFuncD {                                        \
    ALWAYS_INLINE double operator()(double x) { return FUNCD(x); } \
  };                                                               \
  struct NAME##UnaryFuncF {                                        \
    ALWAYS_INLINE float operator()(float x) { return FUNCF(x); }   \
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

template <SHType SHT, typename FuncD, typename FuncF> struct UnaryOperation {
  ALWAYS_INLINE void operate(SHVar &output, const SHVar &input) {
    FuncD fd;
    FuncF ff;
    if constexpr (SHT == SHType::Float) {
      output.valueType = Float;
      output.payload.floatValue = fd(input.payload.floatValue);
    } else if constexpr (SHT == SHType::Float2) {
      output.valueType = Float2;
      output.payload.float2Value[0] = fd(input.payload.float2Value[0]);
      output.payload.float2Value[1] = fd(input.payload.float2Value[1]);
    } else if constexpr (SHT == SHType::Float3) {
      output.valueType = Float3;
      output.payload.float3Value[0] = ff(input.payload.float3Value[0]);
      output.payload.float3Value[1] = ff(input.payload.float3Value[1]);
      output.payload.float3Value[2] = ff(input.payload.float3Value[2]);
    } else if constexpr (SHT == SHType::Float4) {
      output.valueType = Float4;
      output.payload.float4Value[0] = ff(input.payload.float4Value[0]);
      output.payload.float4Value[1] = ff(input.payload.float4Value[1]);
      output.payload.float4Value[2] = ff(input.payload.float4Value[2]);
      output.payload.float4Value[3] = ff(input.payload.float4Value[3]);
    }
  }
};
// End of not used for now

#endif

#define MATH_UNARY_OPERATION(NAME, FUNC, FUNCF)                                                  \
  struct NAME : public UnaryBase {                                                               \
    static SHOptionalString help() {                                                             \
      return SHCCSTR("Calculates `" #NAME "()` on the input value and returns its result, or a " \
                     "sequence of results if input is a sequence.");                             \
    }                                                                                            \
                                                                                                 \
    SHTypeInfo compose(const SHInstanceData &data) {                                             \
      if (data.inputType.basicType == SHType::Seq) {                                             \
        OVERRIDE_ACTIVATE(data, activateSeq);                                                    \
        static_cast<Shard *>(data.shard)->inlineShardId = NotInline;                             \
      } else {                                                                                   \
        OVERRIDE_ACTIVATE(data, activateSingle);                                                 \
        static_cast<Shard *>(data.shard)->inlineShardId = SHInlineShards::Math##NAME;            \
      }                                                                                          \
      return data.inputType;                                                                     \
    }                                                                                            \
                                                                                                 \
    ALWAYS_INLINE void operate(SHVar &output, const SHVar &input) {                              \
      switch (input.valueType) {                                                                 \
      case Float:                                                                                \
        output.valueType = Float;                                                                \
        output.payload.floatValue = FUNC(input.payload.floatValue);                              \
        break;                                                                                   \
      case Float2:                                                                               \
        output.valueType = Float2;                                                               \
        output.payload.float2Value[0] = FUNC(input.payload.float2Value[0]);                      \
        output.payload.float2Value[1] = FUNC(input.payload.float2Value[1]);                      \
        break;                                                                                   \
      case Float3:                                                                               \
        output.valueType = Float3;                                                               \
        output.payload.float3Value[0] = FUNCF(input.payload.float3Value[0]);                     \
        output.payload.float3Value[1] = FUNCF(input.payload.float3Value[1]);                     \
        output.payload.float3Value[2] = FUNCF(input.payload.float3Value[2]);                     \
        break;                                                                                   \
      case Float4:                                                                               \
        output.valueType = Float4;                                                               \
        output.payload.float4Value[0] = FUNCF(input.payload.float4Value[0]);                     \
        output.payload.float4Value[1] = FUNCF(input.payload.float4Value[1]);                     \
        output.payload.float4Value[2] = FUNCF(input.payload.float4Value[2]);                     \
        output.payload.float4Value[3] = FUNCF(input.payload.float4Value[3]);                     \
        break;                                                                                   \
      default:                                                                                   \
        throw ActivationError(#NAME " operation not supported on given types!");                 \
      }                                                                                          \
    }                                                                                            \
                                                                                                 \
    ALWAYS_INLINE SHVar activateSeq(SHContext *context, const SHVar &input) {                    \
      _result.valueType = Seq;                                                                   \
      shards::arrayResize(_result.payload.seqValue, 0);                                          \
      for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {                                \
        SHVar scratch;                                                                           \
        operate(scratch, input.payload.seqValue.elements[i]);                                    \
        shards::arrayPush(_result.payload.seqValue, scratch);                                    \
      }                                                                                          \
      return _result;                                                                            \
    }                                                                                            \
                                                                                                 \
    ALWAYS_INLINE SHVar activateSingle(SHContext *context, const SHVar &input) {                 \
      SHVar scratch;                                                                             \
      operate(scratch, input);                                                                   \
      return scratch;                                                                            \
    }                                                                                            \
                                                                                                 \
    ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {                       \
      throw ActivationError("Invalid activation function");                                      \
    }                                                                                            \
  };                                                                                             \
  RUNTIME_SHARD_TYPE(Math, NAME);

MATH_UNARY_OPERATION(Abs, __builtin_fabs, __builtin_fabsf);
MATH_UNARY_OPERATION(Exp, __builtin_exp, __builtin_expf);
MATH_UNARY_OPERATION(Exp2, __builtin_exp2, __builtin_exp2f);
MATH_UNARY_OPERATION(Expm1, __builtin_expm1, __builtin_expm1f);
MATH_UNARY_OPERATION(Log, __builtin_log, __builtin_logf);
MATH_UNARY_OPERATION(Log10, __builtin_log10, __builtin_log10f);
MATH_UNARY_OPERATION(Log2, __builtin_log2, __builtin_log2f);
MATH_UNARY_OPERATION(Log1p, __builtin_log1p, __builtin_log1pf);
MATH_UNARY_OPERATION(Sqrt, __builtin_sqrt, __builtin_sqrtf);
MATH_UNARY_OPERATION(FastSqrt, fastSqrtNR2, fastSqrtNR2);
MATH_UNARY_OPERATION(FastInvSqrt, fastRcpSqrtNR2, fastRcpSqrtNR2);
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
    double operator()(const SHSeq &seq) {
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
    double operator()(const SHSeq &seq) {
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
    double operator()(const SHSeq &seq) {
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

  static SHOptionalString help() { return SHCCSTR("Calculates the mean of a sequence of floating point numbers."); }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatSeqType; }
  static SHOptionalString inputHelp() { return SHCCSTR("A sequence of floating point numbers."); }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The calculated mean."); }

  static SHParametersInfo parameters() {
    static Type kind{{SHType::Enum, {.enumeration = {CoreCC, 'mean'}}}};
    static Parameters params{{"Kind", SHCCSTR("The kind of Pythagorean means."), {kind}}};
    return params;
  }

  void setParam(int index, const SHVar &value) { mean = MeanKind(value.payload.enumValue); }

  SHVar getParam(int index) { return shards::Var::Enum(mean, CoreCC, 'mean'); }

  SHVar activate(SHContext *context, const SHVar &input) {
    switch (mean) {
    case MeanKind::Arithmetic: {
      ArithMean m;
      return shards::Var(m(input.payload.seqValue));
    }
    case MeanKind::Geometric: {
      GeoMean m;
      return shards::Var(m(input.payload.seqValue));
    }
    case MeanKind::Harmonic: {
      HarmoMean m;
      return shards::Var(m(input.payload.seqValue));
    }
    default:
      throw ActivationError("Invalid mean case.");
    }
  }

  MeanKind mean{MeanKind::Arithmetic};
};

template <class T> struct UnaryBin : public T {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  ParamVar _value{};

  static inline Parameters params{
      {"Value",
       SHCCSTR("The value to increase/decrease."),
       {CoreInfo::IntVarType, CoreInfo::Int2VarType, CoreInfo::Int3VarType, CoreInfo::Int4VarType, CoreInfo::Int8VarType,
        CoreInfo::Int16VarType, CoreInfo::FloatVarType, CoreInfo::Float2VarType, CoreInfo::Float3VarType, CoreInfo::Float4VarType,
        CoreInfo::ColorVarType, CoreInfo::AnyVarSeqType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) { _value = value; }

  SHVar getParam(int index) { return _value; }

  void setOperand(SHType type) {
    switch (type) {
    case SHType::Int:
      T::_operand = shards::Var(1);
      break;
    case SHType::Int2:
      T::_operand = shards::Var(1, 1);
      break;
    case SHType::Int3:
      T::_operand = shards::Var(1, 1, 1);
      break;
    case SHType::Int4:
      T::_operand = shards::Var(1, 1, 1, 1);
      break;
    case SHType::Float:
      T::_operand = shards::Var(1.0);
      break;
    case SHType::Float2:
      T::_operand = shards::Var(1.0, 1.0);
      break;
    case SHType::Float3:
      T::_operand = shards::Var(1.0, 1.0, 1.0);
      break;
    case SHType::Float4:
      T::_operand = shards::Var(1.0, 1.0, 1.0, 1.0);
      break;
    default:
      throw ActivationError("Type not supported for unary math operation");
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (!_value.isVariable()) {
      throw ComposeError("Math.Inc/Dec Expected a variable");
    }

    for (const auto &share : data.shared) {
      if (!strcmp(share.name, _value->payload.stringValue)) {
        if (share.isProtected)
          throw ComposeError("Math.Inc/Dec cannot write protected variables");
        if (!share.isMutable)
          throw ComposeError("Math.Inc/Dec attempt to write immutable variable");
        switch (share.exposedType.basicType) {
        case Seq: {
          if (share.exposedType.seqTypes.len != 1)
            throw ComposeError("Math.Inc/Dec expected a Seq with just one type as input");
          setOperand(share.exposedType.seqTypes.elements[0].basicType);
        } break;
        default:
          setOperand(share.exposedType.basicType);
        }
        SHInstanceData data2 = data;
        data2.inputType = share.exposedType;
        return T::compose(data2);
      }
    }

    throw ComposeError("Math.Inc/Dec variable not found");
  }

  SHExposedTypesInfo requiredVariables() {
    if (_value.isVariable()) {
      _requiredInfo = ExposedInfo(
          ExposedInfo::Variable(_value.variableName(), SHCCSTR("The required operand."), CoreInfo::AnyType));
      return SHExposedTypesInfo(_requiredInfo);
    }
    return {};
  }

  void warmup(SHContext *context) { _value.warmup(context); }

  void cleanup() { _value.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    T::operateFast(T::_opType, _value.get(), _value.get(), T::_operand);
    return input;
  }

  ExposedInfo _requiredInfo{};
};

using Inc = UnaryBin<Add>;
using Dec = UnaryBin<Subtract>;

struct MaxOp final {
  static constexpr bool hasApply{};
  template <typename T> T apply(const T &lhs, const T &rhs) { return std::max<T>(lhs, rhs); }
  ALWAYS_INLINE void operator()(SHVar &output, const SHVar &input, const SHVar &operand, void *) {
    output = std::max(input, operand);
  }
};
using Max = BinaryOperation<MaxOp>;

struct MinOp final {
  static constexpr bool hasApply{};
  template <typename T> T apply(const T &lhs, const T &rhs) { return std::min<T>(lhs, rhs); }
  ALWAYS_INLINE void operator()(SHVar &output, const SHVar &input, const SHVar &operand, void *) {
    output = std::min(input, operand);
  }
};
using Min = BinaryOperation<MinOp>;

#define MATH_BINARY_FLOAT_PROC(NAME, PROC)                                                                  \
  struct NAME##Op final {                                                                                   \
    ALWAYS_INLINE void operator()(SHVar &output, const SHVar &input, const SHVar &operand, void *) {        \
      switch (input.valueType) {                                                                            \
      case Float:                                                                                           \
        output.valueType = Float;                                                                           \
        output.payload.floatValue = PROC(input.payload.floatValue, operand.payload.floatValue);             \
        break;                                                                                              \
      case Float2:                                                                                          \
        output.valueType = Float2;                                                                          \
        output.payload.float2Value[0] = PROC(input.payload.float2Value[0], operand.payload.float2Value[0]); \
        output.payload.float2Value[1] = PROC(input.payload.float2Value[1], operand.payload.float2Value[1]); \
        break;                                                                                              \
      case Float3:                                                                                          \
        output.valueType = Float3;                                                                          \
        output.payload.float3Value[0] = PROC(input.payload.float3Value[0], operand.payload.float3Value[0]); \
        output.payload.float3Value[1] = PROC(input.payload.float3Value[1], operand.payload.float3Value[1]); \
        output.payload.float3Value[2] = PROC(input.payload.float3Value[2], operand.payload.float3Value[2]); \
        break;                                                                                              \
      case Float4:                                                                                          \
        output.valueType = Float4;                                                                          \
        output.payload.float4Value[0] = PROC(input.payload.float4Value[0], operand.payload.float4Value[0]); \
        output.payload.float4Value[1] = PROC(input.payload.float4Value[1], operand.payload.float4Value[1]); \
        output.payload.float4Value[2] = PROC(input.payload.float4Value[2], operand.payload.float4Value[2]); \
        output.payload.float4Value[3] = PROC(input.payload.float4Value[3], operand.payload.float4Value[3]); \
        break;                                                                                              \
      default:                                                                                              \
        throw ActivationError(#NAME " operation not supported between given types!");                       \
      }                                                                                                     \
    }                                                                                                       \
  };                                                                                                        \
  using NAME = BinaryOperation<NAME##Op>;

MATH_BINARY_FLOAT_PROC(Pow, std::pow);
using Pow = BinaryOperation<PowOp>;
MATH_BINARY_FLOAT_PROC(FMod, std::fmod);
using FMod = BinaryOperation<FModOp>;

inline void registerShards() {
  REGISTER_SHARD("Math.Add", Add);
  REGISTER_SHARD("Math.Subtract", Subtract);
  REGISTER_SHARD("Math.Multiply", Multiply);
  REGISTER_SHARD("Math.Divide", Divide);
  REGISTER_SHARD("Math.Xor", Xor);
  REGISTER_SHARD("Math.And", And);
  REGISTER_SHARD("Math.Or", Or);
  REGISTER_SHARD("Math.Mod", Mod);
  REGISTER_SHARD("Math.LShift", LShift);
  REGISTER_SHARD("Math.RShift", RShift);

  REGISTER_SHARD("Math.Abs", Abs);
  REGISTER_SHARD("Math.Exp", Exp);
  REGISTER_SHARD("Math.Exp2", Exp2);
  REGISTER_SHARD("Math.Expm1", Expm1);
  REGISTER_SHARD("Math.Log", Log);
  REGISTER_SHARD("Math.Log10", Log10);
  REGISTER_SHARD("Math.Log2", Log2);
  REGISTER_SHARD("Math.Log1p", Log1p);
  REGISTER_SHARD("Math.Sqrt", Sqrt);
  REGISTER_SHARD("Math.FastSqrt", FastSqrt);
  REGISTER_SHARD("Math.FastInvSqrt", FastInvSqrt);
  REGISTER_SHARD("Math.Cbrt", Cbrt);
  REGISTER_SHARD("Math.Sin", Sin);
  REGISTER_SHARD("Math.Cos", Cos);
  REGISTER_SHARD("Math.Tan", Tan);
  REGISTER_SHARD("Math.Asin", Asin);
  REGISTER_SHARD("Math.Acos", Acos);
  REGISTER_SHARD("Math.Atan", Atan);
  REGISTER_SHARD("Math.Sinh", Sinh);
  REGISTER_SHARD("Math.Cosh", Cosh);
  REGISTER_SHARD("Math.Tanh", Tanh);
  REGISTER_SHARD("Math.Asinh", Asinh);
  REGISTER_SHARD("Math.Acosh", Acosh);
  REGISTER_SHARD("Math.Atanh", Atanh);
  REGISTER_SHARD("Math.Erf", Erf);
  REGISTER_SHARD("Math.Erfc", Erfc);
  REGISTER_SHARD("Math.TGamma", TGamma);
  REGISTER_SHARD("Math.LGamma", LGamma);
  REGISTER_SHARD("Math.Ceil", Ceil);
  REGISTER_SHARD("Math.Floor", Floor);
  REGISTER_SHARD("Math.Trunc", Trunc);
  REGISTER_SHARD("Math.Round", Round);

  REGISTER_SHARD("Math.Mean", Mean);
  REGISTER_SHARD("Math.Inc", Inc);
  REGISTER_SHARD("Math.Dec", Dec);

  REGISTER_SHARD("Max", Max);
  REGISTER_SHARD("Min", Min);
  REGISTER_SHARD("Math.Pow", Pow);
  REGISTER_SHARD("Math.FMod", FMod);
}

}; // namespace Math
}; // namespace shards

#endif // SH_CORE_SHARDS_MATH

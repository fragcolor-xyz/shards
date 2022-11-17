/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

// TODO, remove most of C macros, use more templates

#ifndef SH_CORE_SHARDS_MATH
#define SH_CORE_SHARDS_MATH

#include "foundation.hpp"
#include "shards.h"
#include "shards.hpp"
#include "core.hpp"
#include <sstream>
#include <stdexcept>
#include <variant>
#include "math_ops.hpp"

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

enum OpType {
  Invalid,
  // Operation on scalar & vector type
  Broadcast,
  // Same types
  Direct,
  // Operation on sequence and vector/scalar type matching the sequence element type
  Seq1,
  // Operation on two sequences with the same vector/scalar element type
  SeqSeq
};

struct UnaryBase : public Base {
  OpType _opType = Invalid;

  void validateTypes(const SHTypeInfo &ti) {
    _opType = OpType::Invalid;
    if (ti.basicType == SHType::Seq) {
      _opType = OpType::Seq1;
      if (ti.seqTypes.len != 1)
        throw ComposeError("UnaryVarOperation expected a Seq with just one type as input");
    } else {
      _opType = OpType::Direct;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    validateTypes(data.inputType);
    return data.inputType;
  }
};

struct BinaryBase : public Base {
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

  OpType validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    OpType opType = OpType::Invalid;
    if (rhs != Seq && lhs.basicType == Seq) {
      if (lhs.seqTypes.len != 1 || rhs != lhs.seqTypes.elements[0].basicType)
        throw formatTypeError(lhs.seqTypes.elements[0].basicType, rhs);
      opType = Seq1;
    } else if (rhs == Seq && lhs.basicType == Seq) {
      // TODO need to have deeper types compatibility at least
      opType = SeqSeq;
    }
    return opType;
  }

  template <typename TValidator> SHTypeInfo genericCompose(TValidator &validator, const SHInstanceData &data) {
    SHTypeInfo resultType = data.inputType;
    SHVar operandSpec = _operand;
    if (operandSpec.valueType == ContextVar) {
      bool variableFound = false;
      for (uint32_t i = 0; i < data.shared.len; i++) {
        // normal variable
        if (strcmp(data.shared.elements[i].name, operandSpec.payload.stringValue) == 0) {
          _opType = validator.validateTypes(data.inputType, data.shared.elements[i].exposedType.basicType, resultType);
          variableFound = true;
          break;
        }
      }
      if (!variableFound)
        throw ComposeError(fmt::format("Operand variable {} not found", operandSpec.payload.stringValue));
    } else {
      _opType = validator.validateTypes(data.inputType, operandSpec.valueType, resultType);
    }

    if (_opType == Invalid) {
      throw ComposeError("Incompatible types for binary operation");
    }

    return resultType;
  }

  SHTypeInfo compose(const SHInstanceData &data) { return genericCompose(*this, data); }

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

template <typename TOp> struct ApplyBinary {
  template <SHType ValueType> void apply(SHVarPayload &out, const SHVarPayload &a, const SHVarPayload &b) {
    typename PayloadTraits<ValueType>::ApplyBinary binary{};
    binary.template apply<TOp>(getPayloadContents<ValueType>(out), getPayloadContents<ValueType>(a),
                               getPayloadContents<ValueType>(b));
  }
};

struct ApplyBroadcast {
  template <SHType ValueType> void apply(SHVarPayload &out, const SHVarPayload &a) {
    using PayloadTraits = PayloadTraits<ValueType>;
    typename PayloadTraits::Broadcast broadcast{};
    typename PayloadTraits::UnitType utt{};
    broadcast.template apply(getPayloadContents<ValueType>(out), utt.getContents(const_cast<SHVarPayload &>(a)));
  }
};

template <typename TOp, DispatchType DispatchType = DispatchType::NumberTypes> struct BasicBinaryOperation {
  ApplyBinary<TOp> apply;
  ApplyBroadcast applyBroadcast;
  const VectorTypeTraits *_lhsVecType{};
  const VectorTypeTraits *_rhsVecType{};

  OpType validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    OpType opType = OpType::Invalid;
    if (rhs != Seq && lhs.basicType != Seq) {
      _lhsVecType = VectorTypeLookup::getInstance().get(lhs.basicType);
      _rhsVecType = VectorTypeLookup::getInstance().get(rhs);
      if (_lhsVecType || _rhsVecType) {
        if (!_lhsVecType || !_rhsVecType)
          throw ComposeError("Unsupported type to Math.Multiply");

        bool sameDimension = _lhsVecType->dimension == _rhsVecType->dimension;
        if (!sameDimension && (_lhsVecType->dimension == 1 || _rhsVecType->dimension == 1)) {
          opType = Broadcast;
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
          opType = Direct;
        }
      }
    }

    return opType;
  }

  void operateDirect(SHVar &output, const SHVar &a, const SHVar &b) {
    output.valueType = a.valueType;
    dispatchType<DispatchType>(a.valueType, apply, output.payload, a.payload, b.payload);
  }

  void operateBroadcast(SHVar &output, const SHVar &a, const SHVar &b) {
    // This implements broadcast operators on float types
    const VectorTypeTraits *scalarType = _lhsVecType;
    const VectorTypeTraits *vecType = _rhsVecType;
    SHVarPayload aPayload = a.payload;
    SHVarPayload bPayload = b.payload;

    // Oparands might be swapped (e.g. v*s, s*v)
    if (vecType->dimension == 1) {
      std::swap(vecType, scalarType);
    }

    // Expand scalars to vectors
    if (_lhsVecType->dimension == 1) {
      SHVarPayload temp = aPayload; // Need temp var if input/output are the same
      dispatchType<DispatchType>(vecType->shType, applyBroadcast, aPayload, temp);
    }
    if (_rhsVecType->dimension == 1) {
      SHVarPayload temp = bPayload;
      dispatchType<DispatchType>(vecType->shType, applyBroadcast, bPayload, temp);
    }

    output.valueType = vecType->shType;
    dispatchType<DispatchType>(vecType->shType, apply, output.payload, aPayload, bPayload);
  }
};

///  The Op class has the following interface:
///
///  struct Op {
///    // Return the OpType based on the input types
///    // when OpType::Invalid is results, will fall back to BinaryOperation behaviour
///    // resultType needs to be set when OpType is not OpType::Invalid
///    OpType validateTypes(const SHTypeInfo &a, const SHType &b, SHTypeInfo &resultType);
///    // Apply OpType::Direct
///    void operateDirect(SHVar &output, const SHVar &a, const SHVar &b);
///    // Apply OpType::Broadcast
///    void operateBroadcast(SHVar &output, const SHVar &a, const SHVar &b);
///  };
template <class TOp> struct BinaryOperation : public BinaryBase {
  TOp op;

  static SHOptionalString help() {
    return SHCCSTR("Applies the binary operation on the input value and the operand and returns the result (or a sequence of "
                   "results if the input and the operand are sequences).");
  }

  OpType validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    OpType opType = op.validateTypes(lhs, rhs, resultType);
    if (opType == OpType::Invalid)
      opType = BinaryBase::validateTypes(lhs, rhs, resultType);
    return opType;
  }

  SHTypeInfo compose(const SHInstanceData &data) { return this->genericCompose(*this, data); }

  void operate(OpType opType, SHVar &output, const SHVar &a, const SHVar &b) {
    if (opType == Broadcast) {
      op.operateBroadcast(output, a, b);
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
        auto type = Direct;
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
      if (opType == Direct && output.valueType == Seq) {
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
    if (likely(opType == Direct)) {
      op.operateDirect(output, a, b);
    } else if (opType == Seq1) {
      if (output.valueType != Seq) {
        destroyVar(output);
        output.valueType = Seq;
      }

      shards::arrayResize(output.payload.seqValue, 0);
      for (uint32_t i = 0; i < a.payload.seqValue.len; i++) {
        // notice, we use scratch _output here
        SHVar scratch;
        op.operateDirect(scratch, a.payload.seqValue.elements[i], b);
        shards::arrayPush(output.payload.seqValue, scratch);
      }
    } else {
      operate(_opType, output, a, b);
    }
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    const auto operand = _operand.get();
    operateFast(_opType, _result, input, operand);
    return _result;
  }
};

template <class TOp> struct BinaryIntOperation : public BinaryOperation<TOp> {
  static inline Types IntOrSeqTypes{{CoreInfo::IntType, CoreInfo::Int2Type, CoreInfo::Int3Type, CoreInfo::Int4Type,
                                     CoreInfo::Int8Type, CoreInfo::Int16Type, CoreInfo::ColorType, CoreInfo::AnySeqType}};

  static SHTypesInfo inputTypes() { return IntOrSeqTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Any valid integer(s) or a sequence of such entities supported by this operation.");
  }
  static SHTypesInfo outputTypes() { return IntOrSeqTypes; }
};

template <typename TOp> struct ApplyUnary {
  template <SHType ValueType> void apply(SHVarPayload &out, const SHVarPayload &a) {
    typename PayloadTraits<ValueType>::ApplyUnary unary{};
    unary.template apply<TOp>(getPayloadContents<ValueType>(out), getPayloadContents<ValueType>(a));
  }
};

template <typename TOp, DispatchType DispatchType = DispatchType::NumberTypes> struct BasicUnaryOperation {
  ApplyUnary<TOp> apply;

  OpType validateTypes(const SHTypeInfo &a, SHTypeInfo &resultType) {
    OpType opType = OpType::Invalid;
    return opType;
  }

  void operateDirect(SHVar &output, const SHVar &a) {
    output.valueType = a.valueType;
    dispatchType<DispatchType>(a.valueType, apply, output.payload, a.payload);
  }
};

///  The Op class has the following interface:
///
///  struct Op {
///    // Return the OpType based on the input types
///    // when OpType::Invalid is results, will fall back to BinaryOperation behaviour
///    // resultType needs to be set when OpType is not OpType::Invalid
///    OpType validateTypes(const SHTypeInfo &a, SHTypeInfo &resultType);
///    // Apply OpType::Direct
///    void operateDirect(SHVar &output, const SHVar &a);
///  };
template <class TOp> struct UnaryOperation : public UnaryBase {
  TOp op;

  void destroy() { destroyVar(_result); }

  void validateTypes(const SHTypeInfo &ti, SHTypeInfo &resultType) {
    _opType = op.validateTypes(ti, resultType);
    if (_opType == OpType::Invalid)
      UnaryBase::validateTypes(ti);
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    SHTypeInfo resultType = data.inputType;
    validateTypes(data.inputType, resultType);
    return data.inputType;
  }

  static SHOptionalString help() {
    return SHCCSTR("Applies the unary operation on the input value and returns the result (or a sequence of results if the input "
                   "and the operand are sequences).");
  }

  ALWAYS_INLINE void operate(SHVar &output, const SHVar &a) {
    if (likely(_opType == OpType::Direct)) {
      op.operateDirect(output, a);
    } else if (_opType == OpType::Seq1) {
      if (output.valueType != Seq) {
        destroyVar(output);
        output.valueType = Seq;
      }

      shards::arrayResize(output.payload.seqValue, 0);
      for (uint32_t i = 0; i < a.payload.seqValue.len; i++) {
        // notice, we use scratch _output here
        SHVar scratch;
        op.operateDirect(scratch, a.payload.seqValue.elements[i]);
        shards::arrayPush(output.payload.seqValue, scratch);
      }
    } else {
      throw std::logic_error("Invalid operation type for unary operation");
    }
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    operate(_result, input);
    return _result;
  }
};

template <class TOp> struct UnaryVarOperation final : public UnaryOperation<TOp> {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  ExposedInfo _requiredInfo{};
  ParamVar _value{};

  static inline Parameters params{
      {"Value",
       SHCCSTR("The value to apply the operation to."),
       {CoreInfo::IntVarType, CoreInfo::Int2VarType, CoreInfo::Int3VarType, CoreInfo::Int4VarType, CoreInfo::Int8VarType,
        CoreInfo::Int16VarType, CoreInfo::FloatVarType, CoreInfo::Float2VarType, CoreInfo::Float3VarType, CoreInfo::Float4VarType,
        CoreInfo::ColorVarType, CoreInfo::AnyVarSeqType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) { _value = value; }

  SHVar getParam(int index) { return _value; }

  SHTypeInfo compose(const SHInstanceData &data) {
    SHTypeInfo resultType = data.inputType;

    if (!_value.isVariable()) {
      throw ComposeError("UnaryVarOperation Expected a variable");
    }

    for (const auto &share : data.shared) {
      if (!strcmp(share.name, _value->payload.stringValue)) {
        if (share.isProtected)
          throw ComposeError("UnaryVarOperation cannot write protected variables");
        if (!share.isMutable)
          throw ComposeError("UnaryVarOperation attempt to write immutable variable");

        this->validateTypes(share.exposedType, resultType);
        assert(resultType == data.inputType);

        return resultType;
      }
    }

    throw ComposeError(fmt::format("Math.Inc/Dec variable {} not found", _value->payload.stringValue));
  }

  SHExposedTypesInfo requiredVariables() {
    if (_value.isVariable()) {
      _requiredInfo =
          ExposedInfo(ExposedInfo::Variable(_value.variableName(), SHCCSTR("The required operand."), CoreInfo::AnyType));
      return SHExposedTypesInfo(_requiredInfo);
    }
    return {};
  }

  void warmup(SHContext *context) { _value.warmup(context); }

  void cleanup() { _value.cleanup(); }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    this->operate(_value.get(), _value.get());
    return input;
  }
};

#define MATH_BINARY_OPERATION(NAME, OPERATOR, DIV_BY_ZERO)      \
  using NAME = BinaryOperation<BasicBinaryOperation<NAME##Op>>; \
  RUNTIME_SHARD_TYPE(Math, NAME);

#define MATH_BINARY_INT_OPERATION(NAME, OPERATOR)                                          \
  using NAME = BinaryIntOperation<BasicBinaryOperation<NAME##Op, DispatchType::IntTypes>>; \
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

#define MATH_UNARY_FLOAT_OPERATION(NAME, FUNC, FUNCF)                                   \
  struct NAME##Op final {                                                               \
    template <typename T> T apply(const T &lhs) { return FUNC(lhs); }                   \
  };                                                                                    \
  using NAME = UnaryOperation<BasicUnaryOperation<NAME##Op, DispatchType::FloatTypes>>; \
  RUNTIME_SHARD_TYPE(Math, NAME);

MATH_UNARY_FLOAT_OPERATION(Abs, __builtin_fabs, __builtin_fabsf);
MATH_UNARY_FLOAT_OPERATION(Exp, __builtin_exp, __builtin_expf);
MATH_UNARY_FLOAT_OPERATION(Exp2, __builtin_exp2, __builtin_exp2f);
MATH_UNARY_FLOAT_OPERATION(Expm1, __builtin_expm1, __builtin_expm1f);
MATH_UNARY_FLOAT_OPERATION(Log, __builtin_log, __builtin_logf);
MATH_UNARY_FLOAT_OPERATION(Log10, __builtin_log10, __builtin_log10f);
MATH_UNARY_FLOAT_OPERATION(Log2, __builtin_log2, __builtin_log2f);
MATH_UNARY_FLOAT_OPERATION(Log1p, __builtin_log1p, __builtin_log1pf);
MATH_UNARY_FLOAT_OPERATION(Sqrt, __builtin_sqrt, __builtin_sqrtf);
MATH_UNARY_FLOAT_OPERATION(FastSqrt, fastSqrtNR2, fastSqrtNR2);
MATH_UNARY_FLOAT_OPERATION(FastInvSqrt, fastRcpSqrtNR2, fastRcpSqrtNR2);
MATH_UNARY_FLOAT_OPERATION(Cbrt, __builtin_cbrt, __builtin_cbrt);
MATH_UNARY_FLOAT_OPERATION(Sin, __builtin_sin, __builtin_sinf);
MATH_UNARY_FLOAT_OPERATION(Cos, __builtin_cos, __builtin_cosf);
MATH_UNARY_FLOAT_OPERATION(Tan, __builtin_tan, __builtin_tanf);
MATH_UNARY_FLOAT_OPERATION(Asin, __builtin_asin, __builtin_asinf);
MATH_UNARY_FLOAT_OPERATION(Acos, __builtin_acos, __builtin_acosf);
MATH_UNARY_FLOAT_OPERATION(Atan, __builtin_atan, __builtin_atanf);
MATH_UNARY_FLOAT_OPERATION(Sinh, __builtin_sinh, __builtin_sinhf);
MATH_UNARY_FLOAT_OPERATION(Cosh, __builtin_cosh, __builtin_coshf);
MATH_UNARY_FLOAT_OPERATION(Tanh, __builtin_tanh, __builtin_tanhf);
MATH_UNARY_FLOAT_OPERATION(Asinh, __builtin_asinh, __builtin_asinhf);
MATH_UNARY_FLOAT_OPERATION(Acosh, __builtin_acosh, __builtin_acoshf);
MATH_UNARY_FLOAT_OPERATION(Atanh, __builtin_atanh, __builtin_atanhf);
MATH_UNARY_FLOAT_OPERATION(Erf, __builtin_erf, __builtin_erff);
MATH_UNARY_FLOAT_OPERATION(Erfc, __builtin_erfc, __builtin_erfcf);
MATH_UNARY_FLOAT_OPERATION(TGamma, __builtin_tgamma, __builtin_tgammaf);
MATH_UNARY_FLOAT_OPERATION(LGamma, __builtin_lgamma, __builtin_lgammaf);
MATH_UNARY_FLOAT_OPERATION(Ceil, __builtin_ceil, __builtin_ceilf);
MATH_UNARY_FLOAT_OPERATION(Floor, __builtin_floor, __builtin_floorf);
MATH_UNARY_FLOAT_OPERATION(Trunc, __builtin_trunc, __builtin_truncf);
MATH_UNARY_FLOAT_OPERATION(Round, __builtin_round, __builtin_roundf);

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

struct IncOp final {
  template <typename T> T apply(const T &a) { return a + 1; }
};
using Inc = UnaryVarOperation<BasicUnaryOperation<IncOp>>;

struct DecOp final {
  template <typename T> T apply(const T &a) { return a - 1; }
};
using Dec = UnaryVarOperation<BasicUnaryOperation<DecOp>>;

struct NegateOp final {
  template <typename T> T apply(const T &a) { return -a; }
};
using Negate = UnaryOperation<BasicUnaryOperation<NegateOp>>;

struct MaxOp final {
  template <typename T> T apply(const T &lhs, const T &rhs) { return std::max(lhs, rhs); }
};
using Max = BinaryOperation<BasicBinaryOperation<MaxOp>>;

struct MinOp final {
  template <typename T> T apply(const T &lhs, const T &rhs) { return std::min(lhs, rhs); }
};
using Min = BinaryOperation<BasicBinaryOperation<MinOp>>;

struct PowOp final {
  template <typename T> T apply(const T &lhs, const T &rhs) { return std::pow(lhs, rhs); }
};
using Pow = BinaryOperation<BasicBinaryOperation<PowOp>>;

struct FModOp final {
  template <typename T> T apply(const T &lhs, const T &rhs) { return std::fmod(lhs, rhs); }
};
using FMod = BinaryOperation<BasicBinaryOperation<FModOp>>;

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
  REGISTER_SHARD("Math.Negate", Negate);

  REGISTER_SHARD("Max", Max);
  REGISTER_SHARD("Min", Min);
  REGISTER_SHARD("Math.Pow", Pow);
  REGISTER_SHARD("Math.FMod", FMod);
}

}; // namespace Math
}; // namespace shards

#endif // SH_CORE_SHARDS_MATH

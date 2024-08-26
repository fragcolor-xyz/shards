/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

// TODO, remove most of C macros, use more templates

#ifndef SH_CORE_SHARDS_MATH
#define SH_CORE_SHARDS_MATH

#include <shards/core/foundation.hpp>
#include <shards/core/shards_macros.hpp>
#include <shards/shards.h>
#include <shards/shards.hpp>
#include <shards/common_types.hpp>
#include <shards/number_types.hpp>
#include <sstream>
#include <stdexcept>
#include <variant>
#include <shards/math_ops.hpp>

#define _PC
#include "ShaderFastMathLib.h"
#undef _PC

namespace shards {
namespace Math {
struct Base {
  static inline Types MathTypesNoSeq{{CoreInfo::IntType, CoreInfo::Int2Type, CoreInfo::Int3Type, CoreInfo::Int4Type,
                                      CoreInfo::Int8Type, CoreInfo::Int16Type, CoreInfo::FloatType, CoreInfo::Float2Type,
                                      CoreInfo::Float3Type, CoreInfo::Float4Type, CoreInfo::ColorType}};
  static inline Types MathTypes{MathTypesNoSeq, {CoreInfo::AnySeqType}};

  SHVar _result{};

  void destroy() { destroyVar(_result); }

  SHTypeInfo compose(const SHInstanceData &data) { return data.inputType; }

  static SHTypesInfo inputTypes() { return MathTypes; }

  static SHOptionalString inputHelp() {
    return SHCCSTR("Any valid integer(s), floating point number(s), or a sequence of these types supported by this operation.");
  }

  static SHTypesInfo outputTypes() { return MathTypes; }

  static SHOptionalString outputHelp() {
    return SHCCSTR("The result of the operation, usually in the same type as the input value. If the input is a sequence, the "
                   "output will be a sequence of results, with possible broadcasting according to the input and operand.");
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

  void cleanup(SHContext *context) { _operand.cleanup(); }

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
    if (rhs != SHType::Seq && lhs.basicType == SHType::Seq) {
      if (lhs.seqTypes.len != 1 || rhs != lhs.seqTypes.elements[0].basicType)
        throw formatTypeError(lhs.seqTypes.elements[0].basicType, rhs);
      opType = Seq1;
    } else if (rhs == SHType::Seq && lhs.basicType == SHType::Seq) {
      // TODO need to have deeper types compatibility at least
      opType = SeqSeq;
    }
    return opType;
  }

  template <typename TValidator> SHTypeInfo genericCompose(TValidator &validator, const SHInstanceData &data) {
    SHTypeInfo resultType = data.inputType;
    SHVar operandSpec = _operand;
    if (operandSpec.valueType == SHType::ContextVar) {
      bool variableFound = false;
      for (uint32_t i = 0; i < data.shared.len; i++) {
        // normal variable
        if (data.shared.elements[i].name == SHSTRVIEW(operandSpec)) {
          _opType = validator.validateTypes(data.inputType, data.shared.elements[i].exposedType.basicType, resultType);
          variableFound = true;
          break;
        }
      }
      if (!variableFound)
        throw ComposeError(fmt::format("Operand variable {} not found", SHSTRVIEW(operandSpec)));
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
    // operandSpec should be null terminated cos cloned over
    SHVar operandSpec = _operand;
    if (operandSpec.valueType == SHType::ContextVar) {
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
    broadcast.template apply<>(getPayloadContents<ValueType>(out), utt.getContents(const_cast<SHVarPayload &>(a)));
  }
};

template <typename TOp, DispatchType DispatchType = DispatchType::NumberTypes> struct BasicBinaryOperation {
  static constexpr shards::Math::DispatchType DispatchType_ = DispatchType;

  ApplyBinary<TOp> apply;
  ApplyBroadcast applyBroadcast;
  const VectorTypeTraits *_lhsVecType{};
  const VectorTypeTraits *_rhsVecType{};

  OpType validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    if (rhs != SHType::Seq && lhs.basicType != SHType::Seq) {
      _lhsVecType = VectorTypeLookup::getInstance().get(lhs.basicType);
      _rhsVecType = VectorTypeLookup::getInstance().get(rhs);
      if (_lhsVecType || _rhsVecType) {
        if (!_lhsVecType || !_rhsVecType)
          throw ComposeError(
              fmt::format("Unsupported types to binary operation ({} and {})", type2Name(lhs.basicType), type2Name(rhs)));

        bool sameDimension = _lhsVecType->dimension == _rhsVecType->dimension;
        if (!sameDimension && (_lhsVecType->dimension == 1 || _rhsVecType->dimension == 1)) {
          // Result is the vector type
          if (_rhsVecType->dimension == 1) {
            resultType = _lhsVecType->type;
          } else {
            resultType = _rhsVecType->type;
          }
          return Broadcast;
        } else {
          if (!sameDimension || _lhsVecType->numberType != _rhsVecType->numberType) {
            throw ComposeError(
                fmt::format("Can not multiply vector of size {} and {}", _lhsVecType->dimension, _rhsVecType->dimension));
          }
          return Direct;
        }
      }
    }

    return OpType::Invalid;
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
      if (output.valueType != SHType::Seq) {
        destroyVar(output);
        output.valueType = SHType::Seq;
      }
      // TODO auto-parallelize with taskflow (should be optional)
      auto olen = b.payload.seqValue.len;
      shards::arrayResize(output.payload.seqValue, 0);
      for (uint32_t i = 0; i < a.payload.seqValue.len && olen > 0; i++) {
        const auto &sa = a.payload.seqValue.elements[i];
        const auto &sb = b.payload.seqValue.elements[i % olen];
        auto type = Direct;
        if (likely(sa.valueType == SHType::Seq && sb.valueType == SHType::Seq)) {
          type = SeqSeq;
        } else if (sa.valueType == SHType::Seq && sb.valueType != SHType::Seq) {
          type = Seq1;
        }
        const auto len = output.payload.seqValue.len;
        shards::arrayResize(output.payload.seqValue, len + 1);
        operate(type, output.payload.seqValue.elements[len], sa, sb);
      }
    } else {
      if (opType == Direct && output.valueType == SHType::Seq) {
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
      if (output.valueType != SHType::Seq) {
        destroyVar(output);
        output.valueType = SHType::Seq;
      }

      shards::arrayResize(output.payload.seqValue, 0);
      for (uint32_t i = 0; i < a.payload.seqValue.len; i++) {
        const auto len = output.payload.seqValue.len;
        shards::arrayResize(output.payload.seqValue, len + 1);
        op.operateDirect(output.payload.seqValue.elements[len], a.payload.seqValue.elements[i], b);
      }
    } else {
      operate(_opType, output, a, b);
    }
  }

  ALWAYS_INLINE const SHVar &activate(SHContext *context, const SHVar &input) {
    const auto operand = _operand.get();
    operateFast(_opType, _result, input, operand);
    return _result;
  }
};

template <class TOp> struct BinaryIntOperation : public BinaryOperation<TOp> {
  static inline Types IntOrSeqTypes{{CoreInfo::IntType, CoreInfo::Int2Type, CoreInfo::Int3Type, CoreInfo::Int4Type,
                                     CoreInfo::Int8Type, CoreInfo::Int16Type, CoreInfo::ColorType, CoreInfo::AnySeqType}};

  static SHParametersInfo parameters() {
    static Types ParamTypes = []() {
      static Types types = BinaryBase::MathTypesOrVar;
      if constexpr (hasDispatchType(TOp::DispatchType_, DispatchType::BoolTypes)) {
        types._types.push_back(CoreInfo::BoolType);
        types._types.push_back(CoreInfo::BoolVarType);
      }
      return types;
    }();
    static ParamsInfo Info(ParamsInfo::Param("Operand", SHCCSTR("The operand for this operation."), ParamTypes));
    return SHParametersInfo(Info);
  }

  static SHTypesInfo inputTypes() {
    static Types types = []() {
      Types types = IntOrSeqTypes;
      if constexpr (hasDispatchType(TOp::DispatchType_, DispatchType::BoolTypes)) {
        types._types.push_back(CoreInfo::BoolType);
      }
      return types;
    }();
    return types;
  }
  static SHTypesInfo outputTypes() { return inputTypes(); }

  static SHOptionalString inputHelp() {
    return SHCCSTR("Any valid integer(s) or a sequence of integers supported by this operation.");
  }
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
    return resultType;
  }

  static SHOptionalString help() {
    return SHCCSTR("Applies the unary operation on the input value and returns the result. If the input is a sequence, the "
                   "operation is applied to each element of the sequence.");
  }

  ALWAYS_INLINE void operate(SHVar &output, const SHVar &a) {
    if (likely(_opType == OpType::Direct)) {
      op.operateDirect(output, a);
    } else if (_opType == OpType::Seq1) {
      if (output.valueType != SHType::Seq) {
        destroyVar(output);
        output.valueType = SHType::Seq;
      }

      shards::arrayResize(output.payload.seqValue, 0);
      for (uint32_t i = 0; i < a.payload.seqValue.len; i++) {
        const auto len = output.payload.seqValue.len;
        shards::arrayResize(output.payload.seqValue, len + 1);
        op.operateDirect(output.payload.seqValue.elements[len], a.payload.seqValue.elements[i]);
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

template <class TOp> struct UnaryFloatOperation : public UnaryOperation<TOp> {
  static inline Types FloatOrSeqTypes{{CoreInfo::FloatType, CoreInfo::Float2Type, CoreInfo::Float3Type, CoreInfo::Float4Type,
                                       CoreInfo::ColorType, CoreInfo::AnySeqType}};

  static SHTypesInfo inputTypes() { return FloatOrSeqTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("A floating point number, a vector of floats (Float2, Float3, Float4), a color, or a sequence of these types "
                   "supported by this operation.");
  }
  static SHTypesInfo outputTypes() { return FloatOrSeqTypes; }
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
      if (share.name == SHSTRVIEW((*_value))) {
        if (share.isProtected)
          throw ComposeError("UnaryVarOperation cannot write protected variables");
        if (!share.isMutable)
          throw ComposeError("UnaryVarOperation attempt to write immutable variable");

        this->validateTypes(share.exposedType, resultType);
        assert(resultType == data.inputType);

        return resultType;
      }
    }

    throw ComposeError(fmt::format("Math.Inc/Dec variable {} not found", SHSTRVIEW((*_value))));
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

  void cleanup(SHContext *context) { _value.cleanup(); }

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

#define MATH_BINARY_INT_BOOL_OPERATION(NAME, OPERATOR)                                           \
  using NAME = BinaryIntOperation<BasicBinaryOperation<NAME##Op, DispatchType::IntOrBoolTypes>>; \
  RUNTIME_SHARD_TYPE(Math, NAME);

MATH_BINARY_OPERATION(Add, +, 0);
MATH_BINARY_OPERATION(Subtract, -, 0);
MATH_BINARY_OPERATION(Multiply, *, 0);
MATH_BINARY_OPERATION(Divide, /, 1);
MATH_BINARY_OPERATION(Mod, %, 0);
MATH_BINARY_INT_BOOL_OPERATION(Xor, ^);
MATH_BINARY_INT_BOOL_OPERATION(And, &);
MATH_BINARY_INT_BOOL_OPERATION(Or, |);
MATH_BINARY_INT_OPERATION(LShift, <<);
MATH_BINARY_INT_OPERATION(RShift, >>);

#define MATH_UNARY_OPERATION(NAME, FUNCI, FUNCF)                                         \
  struct NAME##Op final {                                                                \
    template <typename T> T apply(const T &lhs) {                                        \
      if constexpr (std::is_unsigned_v<T>) {                                             \
        return lhs;                                                                      \
      } else if constexpr (std::is_integral_v<T>) {                                      \
        return FUNCI(lhs);                                                               \
      } else {                                                                           \
        return FUNCF(lhs);                                                               \
      }                                                                                  \
    }                                                                                    \
  };                                                                                     \
  using NAME = UnaryOperation<BasicUnaryOperation<NAME##Op, DispatchType::NumberTypes>>; \
  RUNTIME_SHARD_TYPE(Math, NAME);

#define MATH_UNARY_FLOAT_OPERATION(NAME, FUNC, FUNCF)                                        \
  struct NAME##Op final {                                                                    \
    template <typename T> T apply(const T &lhs) { return FUNC(lhs); }                        \
  };                                                                                         \
  using NAME = UnaryFloatOperation<BasicUnaryOperation<NAME##Op, DispatchType::FloatTypes>>; \
  RUNTIME_SHARD_TYPE(Math, NAME);

MATH_UNARY_OPERATION(Abs, __builtin_llabs, __builtin_fabs);
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
  DECL_ENUM_INFO(MeanKind, Mean, 'mean');

  static SHOptionalString help() { return SHCCSTR("Calculates the mean of a sequence of floating point numbers."); }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatSeqType; }
  static SHOptionalString inputHelp() { return SHCCSTR("A sequence of floating point numbers."); }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The calculated mean."); }

  static SHParametersInfo parameters() {
    static Parameters params{{"Kind", SHCCSTR("The kind of Pythagorean means."), {MeanEnumInfo::Type}}};
    return params;
  }

  void setParam(int index, const SHVar &value) { mean = MeanKind(value.payload.enumValue); }

  SHVar getParam(int index) { return shards::Var::Enum(mean, CoreCC, MeanEnumInfo::TypeId); }

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

struct LerpOp final {
  template <typename T> T apply(const T &lhs, const T &rhs, double t) { return T((double)lhs + (double(rhs) - double(lhs)) * t); }
};

struct ApplyLerp final {
  template <SHType ValueType> void apply(SHVarPayload &out, const SHVarPayload &a, const SHVarPayload &b, const SHFloat t) {
    typename PayloadTraits<ValueType>::ApplyBinary binary{};
    binary.template apply<LerpOp>(getPayloadContents<ValueType>(out), getPayloadContents<ValueType>(a),
                                  getPayloadContents<ValueType>(b), t);
  }
};

struct Lerp final {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return Base::MathTypes; }

  static SHOptionalString help() { return SHCCSTR("Linearly interpolate between two values based on input"); }

  static SHParametersInfo parameters() {
    static Parameters params{
        {"First", SHCCSTR("The first value"), BinaryBase::MathTypesOrVar},
        {"Second", SHCCSTR("The second value"), BinaryBase::MathTypesOrVar},
    };
    return params;
  }

  ParamVar _first;
  ParamVar _second;
  Var _result;

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _first = value;
      break;
    case 1:
      _second = value;
      break;
    default:
      throw std::out_of_range("index");
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _first;
    case 1:
      return _second;
    default:
      throw std::out_of_range("index");
    }
  }

  void warmup(SHContext *context) {
    _first.warmup(context);
    _second.warmup(context);
  }

  void cleanup(SHContext *context) {
    _first.cleanup();
    _second.cleanup();
  }

  SHTypeInfo compose(SHInstanceData &data) {
    SHType firstType{};
    SHType secondType{};
    firstType = _first.isVariable() ? findParamVarExposedTypeChecked(data, _first).exposedType.basicType : _first->valueType;
    secondType = _second.isVariable() ? findParamVarExposedTypeChecked(data, _second).exposedType.basicType : _second->valueType;

    if (firstType != secondType)
      throw ComposeError("Types should match");

    return SHTypeInfo{.basicType = firstType};
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    SHVar a = _first.get();
    SHVar b = _second.get();
    SHVar result{.valueType = a.valueType};
    dispatchType<DispatchType::NumberTypes>(a.valueType, ApplyLerp{}, result.payload, a.payload, b.payload,
                                            input.payload.floatValue);
    return result;
  }
};

struct ApplyClamp final {
  template <SHType ValueType>
  void apply(SHVarPayload &out, const SHVarPayload &input, const SHVarPayload &min, const SHVarPayload &max) {
    typename PayloadTraits<ValueType>::ApplyBinary binary{};
    binary.template apply<MaxOp>(getPayloadContents<ValueType>(out), getPayloadContents<ValueType>(input),
                                 getPayloadContents<ValueType>(min));
    binary.template apply<MinOp>(getPayloadContents<ValueType>(out), getPayloadContents<ValueType>(out),
                                 getPayloadContents<ValueType>(max));
  }
};

struct Clamp final {
  static SHTypesInfo inputTypes() { return Base::MathTypesNoSeq; }
  static SHTypesInfo outputTypes() { return Base::MathTypesNoSeq; }

  static SHOptionalString help() { return SHCCSTR("Clamps the input value between the Min and Max values"); }

  static SHParametersInfo parameters() {
    static Parameters params{
        {"Min", SHCCSTR("The first value"), BinaryBase::MathTypesOrVar},
        {"Max", SHCCSTR("The second value"), BinaryBase::MathTypesOrVar},
    };
    return params;
  }

  ParamVar _first;
  ParamVar _second;
  Var _result;

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _first = value;
      break;
    case 1:
      _second = value;
      break;
    default:
      throw std::out_of_range("index");
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _first;
    case 1:
      return _second;
    default:
      throw std::out_of_range("index");
    }
  }

  void warmup(SHContext *context) {
    _first.warmup(context);
    _second.warmup(context);
  }

  void cleanup(SHContext *context) {
    _first.cleanup();
    _second.cleanup();
  }

  SHTypeInfo compose(SHInstanceData &data) {
    SHType firstType{};
    SHType secondType{};
    firstType = _first.isVariable() ? findParamVarExposedTypeChecked(data, _first).exposedType.basicType : _first->valueType;
    secondType = _second.isVariable() ? findParamVarExposedTypeChecked(data, _second).exposedType.basicType : _second->valueType;

    if (firstType != secondType || firstType != data.inputType.basicType)
      throw ComposeError("Types should match");

    return SHTypeInfo{.basicType = firstType};
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    SHVar a = _first.get();
    SHVar b = _second.get();
    SHVar result{.valueType = a.valueType};
    dispatchType<DispatchType::NumberTypes>(a.valueType, ApplyClamp{}, result.payload, input.payload, a.payload, b.payload);
    return result;
  }
};

} // namespace Math
} // namespace shards

#endif // SH_CORE_SHARDS_MATH

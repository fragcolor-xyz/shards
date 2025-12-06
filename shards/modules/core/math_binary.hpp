/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_SHARDS_MATH_BINARY
#define SH_CORE_SHARDS_MATH_BINARY

#include "math_base.hpp"
#include <shards/inlined.hpp>

namespace shards {
namespace Math {

// =============================================================================
// Basic Binary Operation - Template for binary ops with type dispatch
// =============================================================================

template <typename TOp, DispatchType DispatchType_ = DispatchType::NumberTypes> struct BasicBinaryOperation {
  static constexpr shards::Math::DispatchType DispatchType__ = DispatchType_;

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
          throw shards::Error(
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
            throw shards::Error(fmt::format("Can not operate on vector of size {} ({}) and {} ({})", _lhsVecType->dimension,
                                            magic_enum::enum_name(_lhsVecType->numberType), _rhsVecType->dimension,
                                            magic_enum::enum_name(_rhsVecType->numberType)));
          }
          return Direct;
        }
      }
    }

    return OpType::Invalid;
  }

  void operateDirect(SHVar &output, const SHVar &a, const SHVar &b, bool *failed) {
    output.valueType = a.valueType;
    dispatchType<DispatchType_>(a.valueType, failed, apply, output.payload, a.payload, b.payload);
  }

  void operateBroadcast(SHVar &output, const SHVar &a, const SHVar &b, bool *failed) {
    // This implements broadcast operators on float types
    const VectorTypeTraits *scalarType = _lhsVecType;
    const VectorTypeTraits *vecType = _rhsVecType;
    SHVarPayload aPayload = a.payload;
    SHVarPayload bPayload = b.payload;

    // Operands might be swapped (e.g. v*s, s*v)
    if (vecType->dimension == 1) {
      std::swap(vecType, scalarType);
    }

    // Expand scalars to vectors
    if (_lhsVecType->dimension == 1) {
      SHVarPayload temp = aPayload; // Need temp var if input/output are the same
      dispatchType<DispatchType_>(vecType->shType, failed, applyBroadcast, aPayload, temp);
    }
    if (_rhsVecType->dimension == 1) {
      SHVarPayload temp = bPayload;
      dispatchType<DispatchType_>(vecType->shType, failed, applyBroadcast, bPayload, temp);
    }

    output.valueType = vecType->shType;
    dispatchType<DispatchType_>(vecType->shType, failed, apply, output.payload, aPayload, bPayload);
  }
};

// =============================================================================
// Binary Operation - Main template for binary operations
// =============================================================================

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
    return SHCCSTR("Applies the binary operation on the input value and the operand and outputs the result (or a sequence of "
                   "results if the input and the operand are sequences).");
  }

  OpType validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    OpType opType = op.validateTypes(lhs, rhs, resultType);
    if (opType == OpType::Invalid)
      opType = BinaryBase::validateTypes(lhs, rhs, resultType);
    return opType;
  }

  SHTypeInfo composeV2(const SHInstanceData &data) { return this->genericCompose(*this, data); }

  void operate(OpType opType, SHVar &output, const SHVar &a, const SHVar &b, bool *failed) {
    shassert(opType != OpType::Invalid);
    if (opType == Broadcast) {
      op.operateBroadcast(output, a, b, failed);
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
        operate(type, output.payload.seqValue.elements[len], sa, sb, failed);
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
      operateFast(opType, output, a, b, failed);
    }
  }

  ALWAYS_INLINE void operateFast(OpType opType, SHVar &output, const SHVar &a, const SHVar &b, bool *failed) {
    if (likely(opType == Direct)) {
      op.operateDirect(output, a, b, failed);
    } else if (opType == Seq1) {
      if (output.valueType != SHType::Seq) {
        destroyVar(output);
        output.valueType = SHType::Seq;
      }

      shards::arrayResize(output.payload.seqValue, 0);
      for (uint32_t i = 0; i < a.payload.seqValue.len; i++) {
        const auto len = output.payload.seqValue.len;
        shards::arrayResize(output.payload.seqValue, len + 1);
        op.operateDirect(output.payload.seqValue.elements[len], a.payload.seqValue.elements[i], b, failed);
      }
    } else {
      operate(_opType, output, a, b, failed);
    }
  }

  NO_INLINE void cancelFlow(SHContext *context, const SHVar &a, const SHVar &b) {
    context->cancelFlow(fmt::format("Invalid types for Math operation: {} and {}", magic_enum::enum_name(a.valueType),
                                    magic_enum::enum_name(b.valueType)));
  }

  ALWAYS_INLINE const SHVar &activate(SHContext *context, const SHVar &input) {
    const auto operand = _operand.get();
    bool failed = false;
    operateFast(_opType, _result, input, operand, &failed);
    if (failed) {
      cancelFlow(context, input, operand);
    }
    return _result;
  }
};

// =============================================================================
// Binary Int Operation - Integer-only binary operations
// =============================================================================

template <class TOp> struct BinaryIntOperation : public BinaryOperation<TOp> {
  static inline Types IntOrSeqTypes{{CoreInfo::IntType, CoreInfo::Int2Type, CoreInfo::Int3Type, CoreInfo::Int4Type,
                                     CoreInfo::Int8Type, CoreInfo::Int16Type, CoreInfo::ColorType, CoreInfo::AnySeqType}};

  static inline Types IntOrSeqTypesOrBool{{CoreInfo::IntType, CoreInfo::Int2Type, CoreInfo::Int3Type, CoreInfo::Int4Type,
                                           CoreInfo::Int8Type, CoreInfo::Int16Type, CoreInfo::ColorType, CoreInfo::AnySeqType,
                                           CoreInfo::BoolType, CoreInfo::BoolVarType}};

  static SHParametersInfo parameters() {
    static Types ParamTypes = []() {
      static Types types = BinaryBase::MathTypesOrVar;
      if constexpr (hasDispatchType(TOp::DispatchType__, DispatchType::BoolTypes)) {
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
      if constexpr (hasDispatchType(TOp::DispatchType__, DispatchType::BoolTypes)) {
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

// =============================================================================
// Concrete Binary Operations
// =============================================================================

struct Add : public BinaryOperation<BasicBinaryOperation<AddOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard adds the input value to the value provided in the Operand parameter.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The value or the sequence of values to add the value specified in the Operand parameter to.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("This shard outputs the result of the addition."); }

  static SHTypesInfo inputTypes() {
    static Types types{MathTypes, {CoreInfo::AudioType}};
    return types;
  }
  static SHTypesInfo outputTypes() { return inputTypes(); }

  static SHParametersInfo parameters() {
    static Types ParamTypes{MathTypesOrVar,
                            {CoreInfo::AudioType, CoreInfo::AudioVarType, CoreInfo::FloatType, CoreInfo::FloatVarType}};
    static ParamsInfo customParams(
        ParamsInfo::Param("Operand", SHCCSTR("The value or sequence of values to add to the input."), ParamTypes));
    return SHParametersInfo(customParams);
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;

    // Audio path - handle BEFORE genericCompose (which doesn't understand audio)
    if (data.inputType.basicType == SHType::Audio) {
      // Get operand type manually
      SHVar operandSpec = _operand;
      if (operandSpec.valueType == SHType::ContextVar) {
        auto &ctx = CompositionContext::get(data);
        auto varIt = ctx.inherited.find(SHSTRVIEW(operandSpec));
        if (varIt != ctx.inherited.end()) {
          operandType = varIt->second.exposedType;
        } else {
          throw shards::Error(fmt::format("Operand variable \"{}\" not found", SHSTRVIEW(operandSpec)));
        }
      } else {
        operandType.basicType = operandSpec.valueType;
      }

      _dispatchType = SHType::Audio;
      if (operandType.basicType == SHType::Audio) {
        overrideBinaryActivateForType<AddOp, Add, DispatchType::AudioTypes>(data, SHType::Audio, this);
      } else if (operandType.basicType == SHType::Float) {
        overrideBinaryActivateForAudioScalar<AddOp, Add>(data, this);
      } else {
        throw shards::Error("Audio operations require Audio or Float operand");
      }
      return CoreInfo::AudioType;
    }

    auto result = genericCompose(*this, data, &operandType);

    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      // Use InlineShard for super-fast VM path on common types
      if (data.inputType.basicType == SHType::Int || data.inputType.basicType == SHType::Int2) {
        data.shard->inlineShardId = InlineShard::MathAddInt64x2;
      } else if (data.inputType.basicType == SHType::Int3 || data.inputType.basicType == SHType::Int4) {
        data.shard->inlineShardId = InlineShard::MathAddInt32x4;
      } else if (data.inputType.basicType == SHType::Float || data.inputType.basicType == SHType::Float2) {
        data.shard->inlineShardId = InlineShard::MathAddFloat64x2;
      } else if (data.inputType.basicType == SHType::Float3 || data.inputType.basicType == SHType::Float4) {
        data.shard->inlineShardId = InlineShard::MathAddFloat32x4;
      } else {
        // Fallback to OVERRIDE_ACTIVATE for other types (Int8, Int16, Color, etc.)
        overrideBinaryActivateForType<AddOp, Add>(data, data.inputType.basicType, this);
      }
    }
    return result;
  }

  // Fast path methods for inlined.cpp super-fast VM dispatch
  ALWAYS_INLINE void activateInt64x2(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int2Value = input.payload.int2Value + b.payload.int2Value;
  }

  ALWAYS_INLINE void activateInt32x4(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int4Value = input.payload.int4Value + b.payload.int4Value;
  }

  ALWAYS_INLINE void activateFloat64x2(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.float2Value = input.payload.float2Value + b.payload.float2Value;
  }

  ALWAYS_INLINE void activateFloat32x4(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.float4Value = input.payload.float4Value + b.payload.float4Value;
  }
};

struct Subtract : public BinaryOperation<BasicBinaryOperation<SubtractOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard subtracts the value provided in the Operand parameter from the input value.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The value or the sequence of values to subtract the value specified in the Operand parameter from.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("This shard outputs the result of the subtraction."); }

  static SHTypesInfo inputTypes() {
    static Types types{MathTypes, {CoreInfo::AudioType}};
    return types;
  }
  static SHTypesInfo outputTypes() { return inputTypes(); }

  static SHParametersInfo parameters() {
    static Types ParamTypes{MathTypesOrVar,
                            {CoreInfo::AudioType, CoreInfo::AudioVarType, CoreInfo::FloatType, CoreInfo::FloatVarType}};
    static ParamsInfo customParams(
        ParamsInfo::Param("Operand", SHCCSTR("The value or sequence of values to subtract from the input."), ParamTypes));
    return SHParametersInfo(customParams);
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;

    // Audio path - handle BEFORE genericCompose
    if (data.inputType.basicType == SHType::Audio) {
      SHVar operandSpec = _operand;
      if (operandSpec.valueType == SHType::ContextVar) {
        auto &ctx = CompositionContext::get(data);
        auto varIt = ctx.inherited.find(SHSTRVIEW(operandSpec));
        if (varIt != ctx.inherited.end()) {
          operandType = varIt->second.exposedType;
        } else {
          throw shards::Error(fmt::format("Operand variable \"{}\" not found", SHSTRVIEW(operandSpec)));
        }
      } else {
        operandType.basicType = operandSpec.valueType;
      }

      _dispatchType = SHType::Audio;
      if (operandType.basicType == SHType::Audio) {
        overrideBinaryActivateForType<SubtractOp, Subtract, DispatchType::AudioTypes>(data, SHType::Audio, this);
      } else if (operandType.basicType == SHType::Float) {
        overrideBinaryActivateForAudioScalar<SubtractOp, Subtract>(data, this);
      } else {
        throw shards::Error("Audio operations require Audio or Float operand");
      }
      return CoreInfo::AudioType;
    }

    auto result = genericCompose(*this, data, &operandType);

    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      if (data.inputType.basicType == SHType::Int || data.inputType.basicType == SHType::Int2) {
        data.shard->inlineShardId = InlineShard::MathSubtractInt64x2;
      } else if (data.inputType.basicType == SHType::Int3 || data.inputType.basicType == SHType::Int4) {
        data.shard->inlineShardId = InlineShard::MathSubtractInt32x4;
      } else if (data.inputType.basicType == SHType::Float || data.inputType.basicType == SHType::Float2) {
        data.shard->inlineShardId = InlineShard::MathSubtractFloat64x2;
      } else if (data.inputType.basicType == SHType::Float3 || data.inputType.basicType == SHType::Float4) {
        data.shard->inlineShardId = InlineShard::MathSubtractFloat32x4;
      } else {
        overrideBinaryActivateForType<SubtractOp, Subtract>(data, data.inputType.basicType, this);
      }
    }
    return result;
  }

  ALWAYS_INLINE void activateInt64x2(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int2Value = input.payload.int2Value - b.payload.int2Value;
  }

  ALWAYS_INLINE void activateInt32x4(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int4Value = input.payload.int4Value - b.payload.int4Value;
  }

  ALWAYS_INLINE void activateFloat64x2(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.float2Value = input.payload.float2Value - b.payload.float2Value;
  }

  ALWAYS_INLINE void activateFloat32x4(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.float4Value = input.payload.float4Value - b.payload.float4Value;
  }
};

struct Multiply : public BinaryOperation<BasicBinaryOperation<MultiplyOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard multiplies the input value by the value provided in the Operand parameter.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The value or the sequence of values to multiply the value specified in the Operand parameter with.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("This shard outputs the result of the multiplication."); }

  static SHTypesInfo inputTypes() {
    static Types types{MathTypes, {CoreInfo::AudioType}};
    return types;
  }
  static SHTypesInfo outputTypes() { return inputTypes(); }

  static SHParametersInfo parameters() {
    static Types ParamTypes{MathTypesOrVar,
                            {CoreInfo::AudioType, CoreInfo::AudioVarType, CoreInfo::FloatType, CoreInfo::FloatVarType}};
    static ParamsInfo customParams(
        ParamsInfo::Param("Operand", SHCCSTR("The value or sequence of values to multiply the input by."), ParamTypes));
    return SHParametersInfo(customParams);
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;

    // Audio path - handle BEFORE genericCompose
    if (data.inputType.basicType == SHType::Audio) {
      SHVar operandSpec = _operand;
      if (operandSpec.valueType == SHType::ContextVar) {
        auto &ctx = CompositionContext::get(data);
        auto varIt = ctx.inherited.find(SHSTRVIEW(operandSpec));
        if (varIt != ctx.inherited.end()) {
          operandType = varIt->second.exposedType;
        } else {
          throw shards::Error(fmt::format("Operand variable \"{}\" not found", SHSTRVIEW(operandSpec)));
        }
      } else {
        operandType.basicType = operandSpec.valueType;
      }

      _dispatchType = SHType::Audio;
      if (operandType.basicType == SHType::Audio) {
        overrideBinaryActivateForType<MultiplyOp, Multiply, DispatchType::AudioTypes>(data, SHType::Audio, this);
      } else if (operandType.basicType == SHType::Float) {
        overrideBinaryActivateForAudioScalar<MultiplyOp, Multiply>(data, this);
      } else {
        throw shards::Error("Audio operations require Audio or Float operand");
      }
      return CoreInfo::AudioType;
    }

    auto result = genericCompose(*this, data, &operandType);

    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      if (data.inputType.basicType == SHType::Int || data.inputType.basicType == SHType::Int2) {
        data.shard->inlineShardId = InlineShard::MathMultiplyInt64x2;
      } else if (data.inputType.basicType == SHType::Int3 || data.inputType.basicType == SHType::Int4) {
        data.shard->inlineShardId = InlineShard::MathMultiplyInt32x4;
      } else if (data.inputType.basicType == SHType::Float || data.inputType.basicType == SHType::Float2) {
        data.shard->inlineShardId = InlineShard::MathMultiplyFloat64x2;
      } else if (data.inputType.basicType == SHType::Float3 || data.inputType.basicType == SHType::Float4) {
        data.shard->inlineShardId = InlineShard::MathMultiplyFloat32x4;
      } else {
        overrideBinaryActivateForType<MultiplyOp, Multiply>(data, data.inputType.basicType, this);
      }
    }
    return result;
  }

  ALWAYS_INLINE void activateInt64x2(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int2Value = input.payload.int2Value * b.payload.int2Value;
  }

  ALWAYS_INLINE void activateInt32x4(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int4Value = input.payload.int4Value * b.payload.int4Value;
  }

  ALWAYS_INLINE void activateFloat64x2(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.float2Value = input.payload.float2Value * b.payload.float2Value;
  }

  ALWAYS_INLINE void activateFloat32x4(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.float4Value = input.payload.float4Value * b.payload.float4Value;
  }
};

struct Divide : public BinaryOperation<BasicBinaryOperation<DivideOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard divides the input value by the value provided in the Operand parameter.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The value or the sequence of values to divide the value specified in the Operand parameter with.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("This shard outputs the result of the division."); }

  static SHTypesInfo inputTypes() {
    static Types types{MathTypes, {CoreInfo::AudioType}};
    return types;
  }
  static SHTypesInfo outputTypes() { return inputTypes(); }

  static SHParametersInfo parameters() {
    static Types ParamTypes{MathTypesOrVar,
                            {CoreInfo::AudioType, CoreInfo::AudioVarType, CoreInfo::FloatType, CoreInfo::FloatVarType}};
    static ParamsInfo customParams(
        ParamsInfo::Param("Operand", SHCCSTR("The value or sequence of values to divide the input by."), ParamTypes));
    return SHParametersInfo(customParams);
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;

    // Audio path - handle BEFORE genericCompose
    if (data.inputType.basicType == SHType::Audio) {
      SHVar operandSpec = _operand;
      if (operandSpec.valueType == SHType::ContextVar) {
        auto &ctx = CompositionContext::get(data);
        auto varIt = ctx.inherited.find(SHSTRVIEW(operandSpec));
        if (varIt != ctx.inherited.end()) {
          operandType = varIt->second.exposedType;
        } else {
          throw shards::Error(fmt::format("Operand variable \"{}\" not found", SHSTRVIEW(operandSpec)));
        }
      } else {
        operandType.basicType = operandSpec.valueType;
      }

      _dispatchType = SHType::Audio;
      if (operandType.basicType == SHType::Audio) {
        overrideBinaryActivateForType<DivideOp, Divide, DispatchType::AudioTypes>(data, SHType::Audio, this);
      } else if (operandType.basicType == SHType::Float) {
        overrideBinaryActivateForAudioScalar<DivideOp, Divide>(data, this);
      } else {
        throw shards::Error("Audio operations require Audio or Float operand");
      }
      return CoreInfo::AudioType;
    }

    auto result = genericCompose(*this, data, &operandType);

    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      overrideBinaryActivateForType<DivideOp, Divide>(data, data.inputType.basicType, this);
    }
    return result;
  }
};

struct Mod : public BinaryOperation<BasicBinaryOperation<ModOp>> {
  static SHOptionalString help() {
    return SHCCSTR(
        "This shard calculates the remainder of the division of the input value by the value provided in the Operand parameter.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The value or the sequence of values to divide the value specified in the Operand parameter with.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("This shard outputs the result of the modulus operation."); }

  static SHParametersInfo parameters() {
    static ParamsInfo customParams(ParamsInfo::Param(
        "Operand", SHCCSTR("The value or sequence of values to divide the input by and get the remainder of."), MathTypesOrVar));
    return SHParametersInfo(customParams);
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;
    auto result = genericCompose(*this, data, &operandType);
    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      overrideBinaryActivateForType<ModOp, Mod>(data, data.inputType.basicType, this);
    }
    return result;
  }
};

struct Xor : public BinaryIntOperation<BasicBinaryOperation<XorOp, DispatchType::IntOrBoolTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard performs a bitwise XOR operation on the input with the value specified in the "
                   "Operand parameter and outputs the result.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The value or the sequence of values to compare the value specified in the Operand parameter with.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("This shard outputs the value resulting from the XOR operation."); }

  static SHParametersInfo parameters() {
    static ParamsInfo customParams(
        ParamsInfo::Param("Operand", SHCCSTR("The value or sequence of values to compare the input with."), IntOrSeqTypesOrBool));
    return SHParametersInfo(customParams);
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;
    auto result = genericCompose(*this, data, &operandType);
    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      if (data.inputType.basicType == SHType::Int || data.inputType.basicType == SHType::Int2) {
        data.shard->inlineShardId = InlineShard::MathXorInt64x2;
      } else if (data.inputType.basicType == SHType::Int3 || data.inputType.basicType == SHType::Int4) {
        data.shard->inlineShardId = InlineShard::MathXorInt32x4;
      } else {
        overrideBinaryActivateForType<XorOp, Xor, DispatchType::IntOrBoolTypes>(data, data.inputType.basicType, this);
      }
    }
    return result;
  }

  ALWAYS_INLINE void activateInt64x2(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int2Value = input.payload.int2Value ^ b.payload.int2Value;
  }

  ALWAYS_INLINE void activateInt32x4(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int4Value = input.payload.int4Value ^ b.payload.int4Value;
  }
};

struct And : public BinaryIntOperation<BasicBinaryOperation<AndOp, DispatchType::IntOrBoolTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard performs a bitwise AND operation on the input value with the value specified in the "
                   "Operand parameter and outputs the result.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The value or the sequence of values to compare the value specified in the Operand parameter with.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("This shard outputs the value resulting from the AND operation."); }

  static SHParametersInfo parameters() {
    static ParamsInfo customParams(
        ParamsInfo::Param("Operand", SHCCSTR("The value or sequence of values to compare the input with."), IntOrSeqTypesOrBool));
    return SHParametersInfo(customParams);
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;
    auto result = genericCompose(*this, data, &operandType);
    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      if (data.inputType.basicType == SHType::Int || data.inputType.basicType == SHType::Int2) {
        data.shard->inlineShardId = InlineShard::MathAndInt64x2;
      } else if (data.inputType.basicType == SHType::Int3 || data.inputType.basicType == SHType::Int4) {
        data.shard->inlineShardId = InlineShard::MathAndInt32x4;
      } else {
        overrideBinaryActivateForType<AndOp, And, DispatchType::IntOrBoolTypes>(data, data.inputType.basicType, this);
      }
    }
    return result;
  }

  ALWAYS_INLINE void activateInt64x2(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int2Value = input.payload.int2Value & b.payload.int2Value;
  }

  ALWAYS_INLINE void activateInt32x4(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int4Value = input.payload.int4Value & b.payload.int4Value;
  }
};

struct Or : public BinaryIntOperation<BasicBinaryOperation<OrOp, DispatchType::IntOrBoolTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard performs a bitwise OR operation on the input value with the value specified in the "
                   "Operand parameter and outputs the result.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The value or the sequence of values to compare the value specified in the Operand parameter with.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("This shard outputs the value resulting from the OR operation."); }

  static SHParametersInfo parameters() {
    static ParamsInfo customParams(
        ParamsInfo::Param("Operand", SHCCSTR("The value or sequence of values to compare the input with."), IntOrSeqTypesOrBool));
    return SHParametersInfo(customParams);
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;
    auto result = genericCompose(*this, data, &operandType);
    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      if (data.inputType.basicType == SHType::Int || data.inputType.basicType == SHType::Int2) {
        data.shard->inlineShardId = InlineShard::MathOrInt64x2;
      } else if (data.inputType.basicType == SHType::Int3 || data.inputType.basicType == SHType::Int4) {
        data.shard->inlineShardId = InlineShard::MathOrInt32x4;
      } else {
        overrideBinaryActivateForType<OrOp, Or, DispatchType::IntOrBoolTypes>(data, data.inputType.basicType, this);
      }
    }
    return result;
  }

  ALWAYS_INLINE void activateInt64x2(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int2Value = input.payload.int2Value | b.payload.int2Value;
  }

  ALWAYS_INLINE void activateInt32x4(const SHVar &input) {
    auto b = _operand.get();
    _result.payload.int4Value = input.payload.int4Value | b.payload.int4Value;
  }
};

struct LShift : public BinaryIntOperation<BasicBinaryOperation<LShiftOp, DispatchType::IntTypes>> {
  static SHOptionalString help() {
    return SHCCSTR(
        "This shard shifts the bits of the input value to the left by the number of positions specified in the Operand "
        "parameter.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The integer or the sequence of integers to shift the bits of."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the value resulting from the left shift operation."); }

  static SHParametersInfo parameters() {
    static ParamsInfo customParams(ParamsInfo::Param(
        "Operand", SHCCSTR("The number of positions to shift the bits of the input value to the left by."), IntOrSeqTypes));
    return SHParametersInfo(customParams);
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;
    auto result = genericCompose(*this, data, &operandType);
    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      overrideBinaryActivateForType<LShiftOp, LShift, DispatchType::IntTypes>(data, data.inputType.basicType, this);
    }
    return result;
  }
};

struct RShift : public BinaryIntOperation<BasicBinaryOperation<RShiftOp, DispatchType::IntTypes>> {
  static SHOptionalString help() {
    return SHCCSTR(
        "This shard shifts the bits of the input value to the right by the number of positions specified in the Operand "
        "parameter.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The integer or the sequence of integers to shift the bits of."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the value resulting from the right shift operation."); }

  static SHParametersInfo parameters() {
    static ParamsInfo customParams(ParamsInfo::Param(
        "Operand", SHCCSTR("The number of positions to shift the bits of the input value to the right by."), IntOrSeqTypes));
    return SHParametersInfo(customParams);
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;
    auto result = genericCompose(*this, data, &operandType);
    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      overrideBinaryActivateForType<RShiftOp, RShift, DispatchType::IntTypes>(data, data.inputType.basicType, this);
    }
    return result;
  }
};

struct MaxOp {
  template <typename T> T apply(const T &lhs, const T &rhs) { return std::max(lhs, rhs); }
};
struct Max : public BinaryOperation<BasicBinaryOperation<MaxOp>> {
  static SHOptionalString help() {
    return SHCCSTR(
        "This shard compares the input with the value specified in the `Operand` parameter and outputs the larger value.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The first value to compare with."); }

  static SHOptionalString outputHelp() { return SHCCSTR("The larger value between the input and the operand."); }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;
    auto result = genericCompose(*this, data, &operandType);
    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      overrideBinaryActivateForType<MaxOp, Max>(data, data.inputType.basicType, this);
    }
    return result;
  }
};

struct MinOp final {
  template <typename T> T apply(const T &lhs, const T &rhs) { return std::min(lhs, rhs); }
};
struct Min : public BinaryOperation<BasicBinaryOperation<MinOp>> {
  static SHOptionalString help() {
    return SHCCSTR(
        "This shard compares the input with the value specified in the `Operand` parameter and outputs the smaller value.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The first value to compare with."); }

  static SHOptionalString outputHelp() { return SHCCSTR("The smaller value between the input and the operand."); }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;
    auto result = genericCompose(*this, data, &operandType);
    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      overrideBinaryActivateForType<MinOp, Min>(data, data.inputType.basicType, this);
    }
    return result;
  }
};

struct PowOp final {
  template <typename T> T apply(const T &lhs, const T &rhs) { return std::pow(lhs, rhs); }
};
struct Pow : public BinaryOperation<BasicBinaryOperation<PowOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard raises the input to the power of the exponent specified in the Operand parameter.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The base value to raise the power of."); }

  static SHOptionalString outputHelp() { return SHCCSTR("The result of raising the input to the power of the operand."); }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;
    auto result = genericCompose(*this, data, &operandType);
    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      overrideBinaryActivateForType<PowOp, Pow>(data, data.inputType.basicType, this);
    }
    return result;
  }
};

struct Atan2Op final {
  template <typename T> T apply(const T &y, const T &x) { return std::atan2(y, x); }
};
struct Atan2 : public BinaryOperation<BasicBinaryOperation<Atan2Op>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the angle in radians whose tangent is the quotient of the two inputs.");
  }
  static SHOptionalString inputHelp() {
    return SHCCSTR("The first input is the y-coordinate, and the second input is the x-coordinate.");
  }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs the angle in radians whose tangent is the quotient of the two inputs.");
  }

  SHTypeInfo composeV2(const SHInstanceData &data) {
    SHTypeInfo operandType{};
    _dispatchType = SHType::None;
    auto result = genericCompose(*this, data, &operandType);
    if (_opType == Direct && data.inputType == operandType) {
      _dispatchType = data.inputType.basicType;
      overrideBinaryActivateForType<Atan2Op, Atan2>(data, data.inputType.basicType, this);
    }
    return result;
  }
};

} // namespace Math
} // namespace shards

#endif // SH_CORE_SHARDS_MATH_BINARY

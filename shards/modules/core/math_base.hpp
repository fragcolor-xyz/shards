/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_SHARDS_MATH_BASE
#define SH_CORE_SHARDS_MATH_BASE

#include <shards/core/foundation.hpp>
#include <shards/core/shards_macros.hpp>
#include <shards/shards.h>
#include <shards/shards.hpp>
#include <shards/common_types.hpp>
#include <shards/number_types.hpp>
#include <shards/math_ops.hpp>
#include <shards/core/params.hpp>
#include <shards/shardwrapper.hpp>
#include <sstream>
#include <stdexcept>
#include <variant>

// SIMD headers
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#elif defined(__AVX2__) || defined(__SSE__)
#include <immintrin.h>
#endif

// SLEEF for vectorized transcendentals (only when audio module is enabled)
#if __has_include(<sleef.h>)
#include <sleef.h>
#define SHARDS_HAS_SLEEF 1
#endif

// Audio buffer capacity tracking via SHVar.version field
// The version field is unused for Audio type, so we repurpose it to store buffer capacity
#define SHVAR_AUDIO_GET_CAPACITY(var) ((var).version)
#define SHVAR_AUDIO_SET_CAPACITY(var, cap) ((var).version = (cap))

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

// =============================================================================
// Typed Dispatch - Function pointer based dispatch resolved at compose time
// =============================================================================

// Function signature for binary operations: (output, input_a, input_b)
using BinaryDispatchFn = void (*)(SHVarPayload &, const SHVarPayload &, const SHVarPayload &);

// Function signature for unary operations: (output, input)
using UnaryDispatchFn = void (*)(SHVarPayload &, const SHVarPayload &);

// Typed binary operation that can be stored as function pointer
template <typename TOp, SHType ValueType>
void typedBinaryOp(SHVarPayload &out, const SHVarPayload &a, const SHVarPayload &b) {
  typename PayloadTraits<ValueType>::ApplyBinary binary{};
  binary.template apply<TOp>(getPayloadContents<ValueType>(out), getPayloadContents<ValueType>(a),
                             getPayloadContents<ValueType>(b));
}

// Typed unary operation that can be stored as function pointer
template <typename TOp, SHType ValueType>
void typedUnaryOp(SHVarPayload &out, const SHVarPayload &a) {
  typename PayloadTraits<ValueType>::ApplyUnary unary{};
  unary.template apply<TOp>(getPayloadContents<ValueType>(out), getPayloadContents<ValueType>(a));
}

// =============================================================================
// Audio SIMD Operations - Optimized element-wise operations for planar audio
// =============================================================================

// Forward declarations for audio ops
struct AddOp;
struct SubtractOp;
struct MultiplyOp;
struct DivideOp;

// SIMD binary audio add
inline void applyBinaryAudioAdd(float *__restrict out, const float *__restrict a, const float *__restrict b, size_t count) {
  size_t i = 0;
#if defined(__AVX2__)
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i);
    __m256 vb = _mm256_loadu_ps(b + i);
    _mm256_storeu_ps(out + i, _mm256_add_ps(va, vb));
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    float32x4_t vb = vld1q_f32(b + i);
    vst1q_f32(out + i, vaddq_f32(va, vb));
  }
#endif
  for (; i < count; ++i)
    out[i] = a[i] + b[i];
}

// SIMD binary audio subtract
inline void applyBinaryAudioSubtract(float *__restrict out, const float *__restrict a, const float *__restrict b, size_t count) {
  size_t i = 0;
#if defined(__AVX2__)
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i);
    __m256 vb = _mm256_loadu_ps(b + i);
    _mm256_storeu_ps(out + i, _mm256_sub_ps(va, vb));
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    float32x4_t vb = vld1q_f32(b + i);
    vst1q_f32(out + i, vsubq_f32(va, vb));
  }
#endif
  for (; i < count; ++i)
    out[i] = a[i] - b[i];
}

// SIMD binary audio multiply
inline void applyBinaryAudioMultiply(float *__restrict out, const float *__restrict a, const float *__restrict b, size_t count) {
  size_t i = 0;
#if defined(__AVX2__)
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i);
    __m256 vb = _mm256_loadu_ps(b + i);
    _mm256_storeu_ps(out + i, _mm256_mul_ps(va, vb));
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    float32x4_t vb = vld1q_f32(b + i);
    vst1q_f32(out + i, vmulq_f32(va, vb));
  }
#endif
  for (; i < count; ++i)
    out[i] = a[i] * b[i];
}

// SIMD binary audio divide
inline void applyBinaryAudioDivide(float *__restrict out, const float *__restrict a, const float *__restrict b, size_t count) {
  size_t i = 0;
#if defined(__AVX2__)
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i);
    __m256 vb = _mm256_loadu_ps(b + i);
    _mm256_storeu_ps(out + i, _mm256_div_ps(va, vb));
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    float32x4_t vb = vld1q_f32(b + i);
    vst1q_f32(out + i, vdivq_f32(va, vb));
  }
#endif
  for (; i < count; ++i)
    out[i] = a[i] / b[i];
}

// Template dispatch for binary audio ops
template <typename TOp>
inline void applyBinaryAudioOp(float *out, const float *a, const float *b, size_t count);

template <>
inline void applyBinaryAudioOp<AddOp>(float *out, const float *a, const float *b, size_t count) {
  applyBinaryAudioAdd(out, a, b, count);
}
template <>
inline void applyBinaryAudioOp<SubtractOp>(float *out, const float *a, const float *b, size_t count) {
  applyBinaryAudioSubtract(out, a, b, count);
}
template <>
inline void applyBinaryAudioOp<MultiplyOp>(float *out, const float *a, const float *b, size_t count) {
  applyBinaryAudioMultiply(out, a, b, count);
}
template <>
inline void applyBinaryAudioOp<DivideOp>(float *out, const float *a, const float *b, size_t count) {
  applyBinaryAudioDivide(out, a, b, count);
}

// SIMD scalar broadcast versions
inline void applyBinaryAudioAddScalar(float *__restrict out, const float *__restrict a, float scalar, size_t count) {
  size_t i = 0;
#if defined(__AVX2__)
  __m256 vs = _mm256_set1_ps(scalar);
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i);
    _mm256_storeu_ps(out + i, _mm256_add_ps(va, vs));
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  float32x4_t vs = vdupq_n_f32(scalar);
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    vst1q_f32(out + i, vaddq_f32(va, vs));
  }
#endif
  for (; i < count; ++i)
    out[i] = a[i] + scalar;
}

inline void applyBinaryAudioSubtractScalar(float *__restrict out, const float *__restrict a, float scalar, size_t count) {
  size_t i = 0;
#if defined(__AVX2__)
  __m256 vs = _mm256_set1_ps(scalar);
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i);
    _mm256_storeu_ps(out + i, _mm256_sub_ps(va, vs));
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  float32x4_t vs = vdupq_n_f32(scalar);
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    vst1q_f32(out + i, vsubq_f32(va, vs));
  }
#endif
  for (; i < count; ++i)
    out[i] = a[i] - scalar;
}

inline void applyBinaryAudioMultiplyScalar(float *__restrict out, const float *__restrict a, float scalar, size_t count) {
  size_t i = 0;
#if defined(__AVX2__)
  __m256 vs = _mm256_set1_ps(scalar);
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i);
    _mm256_storeu_ps(out + i, _mm256_mul_ps(va, vs));
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  float32x4_t vs = vdupq_n_f32(scalar);
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    vst1q_f32(out + i, vmulq_f32(va, vs));
  }
#endif
  for (; i < count; ++i)
    out[i] = a[i] * scalar;
}

inline void applyBinaryAudioDivideScalar(float *__restrict out, const float *__restrict a, float scalar, size_t count) {
  size_t i = 0;
#if defined(__AVX2__)
  __m256 vs = _mm256_set1_ps(scalar);
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i);
    _mm256_storeu_ps(out + i, _mm256_div_ps(va, vs));
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  float32x4_t vs = vdupq_n_f32(scalar);
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    vst1q_f32(out + i, vdivq_f32(va, vs));
  }
#endif
  for (; i < count; ++i)
    out[i] = a[i] / scalar;
}

// Template dispatch for scalar broadcast ops
template <typename TOp>
inline void applyBinaryAudioScalarOp(float *out, const float *a, float scalar, size_t count);

template <>
inline void applyBinaryAudioScalarOp<AddOp>(float *out, const float *a, float scalar, size_t count) {
  applyBinaryAudioAddScalar(out, a, scalar, count);
}
template <>
inline void applyBinaryAudioScalarOp<SubtractOp>(float *out, const float *a, float scalar, size_t count) {
  applyBinaryAudioSubtractScalar(out, a, scalar, count);
}
template <>
inline void applyBinaryAudioScalarOp<MultiplyOp>(float *out, const float *a, float scalar, size_t count) {
  applyBinaryAudioMultiplyScalar(out, a, scalar, count);
}
template <>
inline void applyBinaryAudioScalarOp<DivideOp>(float *out, const float *a, float scalar, size_t count) {
  applyBinaryAudioDivideScalar(out, a, scalar, count);
}

// Unary audio op - scalar loop (compiler may auto-vectorize for simple ops)
template <typename TOp>
inline void applyUnaryAudioOp(float *out, const float *a, size_t count) {
  TOp op{};
  for (size_t i = 0; i < count; ++i) {
    out[i] = op.template apply<float>(a[i]);
  }
}

// Get function pointer for a binary operation given the type
template <typename TOp, DispatchType DT>
constexpr BinaryDispatchFn getBinaryDispatchFn(SHType type) {
  switch (type) {
  case SHType::Int:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedBinaryOp<TOp, SHType::Int>;
    break;
  case SHType::Int2:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedBinaryOp<TOp, SHType::Int2>;
    break;
  case SHType::Int3:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedBinaryOp<TOp, SHType::Int3>;
    break;
  case SHType::Int4:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedBinaryOp<TOp, SHType::Int4>;
    break;
  case SHType::Int8:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedBinaryOp<TOp, SHType::Int8>;
    break;
  case SHType::Int16:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedBinaryOp<TOp, SHType::Int16>;
    break;
  case SHType::Color:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedBinaryOp<TOp, SHType::Color>;
    break;
  case SHType::Float:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes))
      return &typedBinaryOp<TOp, SHType::Float>;
    break;
  case SHType::Float2:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes))
      return &typedBinaryOp<TOp, SHType::Float2>;
    break;
  case SHType::Float3:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes))
      return &typedBinaryOp<TOp, SHType::Float3>;
    break;
  case SHType::Float4:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes))
      return &typedBinaryOp<TOp, SHType::Float4>;
    break;
  case SHType::Bool:
    if constexpr (hasDispatchType(DT, DispatchType::BoolTypes))
      return &typedBinaryOp<TOp, SHType::Bool>;
    break;
  default:
    break;
  }
  return nullptr;
}

// Get function pointer for a unary operation given the type
template <typename TOp, DispatchType DT>
constexpr UnaryDispatchFn getUnaryDispatchFn(SHType type) {
  switch (type) {
  case SHType::Int:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedUnaryOp<TOp, SHType::Int>;
    break;
  case SHType::Int2:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedUnaryOp<TOp, SHType::Int2>;
    break;
  case SHType::Int3:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedUnaryOp<TOp, SHType::Int3>;
    break;
  case SHType::Int4:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedUnaryOp<TOp, SHType::Int4>;
    break;
  case SHType::Int8:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedUnaryOp<TOp, SHType::Int8>;
    break;
  case SHType::Int16:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedUnaryOp<TOp, SHType::Int16>;
    break;
  case SHType::Color:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes))
      return &typedUnaryOp<TOp, SHType::Color>;
    break;
  case SHType::Float:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes))
      return &typedUnaryOp<TOp, SHType::Float>;
    break;
  case SHType::Float2:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes))
      return &typedUnaryOp<TOp, SHType::Float2>;
    break;
  case SHType::Float3:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes))
      return &typedUnaryOp<TOp, SHType::Float3>;
    break;
  case SHType::Float4:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes))
      return &typedUnaryOp<TOp, SHType::Float4>;
    break;
  case SHType::Bool:
    if constexpr (hasDispatchType(DT, DispatchType::BoolTypes))
      return &typedUnaryOp<TOp, SHType::Bool>;
    break;
  default:
    break;
  }
  return nullptr;
}

// =============================================================================
// Unary Base
// =============================================================================

struct UnaryBase : public Base {
  OpType _opType = Invalid;
  SHType _dispatchType{SHType::None};  // Resolved type for direct dispatch

  void validateTypes(const SHTypeInfo &ti) {
    _opType = OpType::Invalid;
    if (ti.basicType == SHType::Seq) {
      _opType = OpType::Seq1;
      if (ti.seqTypes.len != 1)
        throw shards::Error("UnaryVarOperation expected a Seq with just one type as input");
    } else {
      _opType = OpType::Direct;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    validateTypes(data.inputType);
    return data.inputType;
  }
};

// =============================================================================
// Binary Base  
// =============================================================================

struct BinaryBase : public Base {
  static inline Types MathTypesOrVar{
      {CoreInfo::IntType,       CoreInfo::IntVarType,   CoreInfo::Int2Type,      CoreInfo::Int2VarType,  CoreInfo::Int3Type,
       CoreInfo::Int3VarType,   CoreInfo::Int4Type,     CoreInfo::Int4VarType,   CoreInfo::Int8Type,     CoreInfo::Int8VarType,
       CoreInfo::Int16Type,     CoreInfo::Int16VarType, CoreInfo::FloatType,     CoreInfo::FloatVarType, CoreInfo::Float2Type,
       CoreInfo::Float2VarType, CoreInfo::Float3Type,   CoreInfo::Float3VarType, CoreInfo::Float4Type,   CoreInfo::Float4VarType,
       CoreInfo::ColorType,     CoreInfo::ColorVarType, CoreInfo::AnySeqType,    CoreInfo::AnyVarSeqType}};

  static inline Types OnlyNumbers{{CoreInfo::IntType,    CoreInfo::IntVarType,    CoreInfo::Int2Type,   CoreInfo::Int2VarType,
                                   CoreInfo::Int3Type,   CoreInfo::Int3VarType,   CoreInfo::Int4Type,   CoreInfo::Int4VarType,
                                   CoreInfo::Int8Type,   CoreInfo::Int8VarType,   CoreInfo::Int16Type,  CoreInfo::Int16VarType,
                                   CoreInfo::FloatType,  CoreInfo::FloatVarType,  CoreInfo::Float2Type, CoreInfo::Float2VarType,
                                   CoreInfo::Float3Type, CoreInfo::Float3VarType, CoreInfo::Float4Type, CoreInfo::Float4VarType,
                                   CoreInfo::ColorType,  CoreInfo::ColorVarType}};

  static inline ParamsInfo mathParamsInfo =
      ParamsInfo(ParamsInfo::Param("Operand", SHCCSTR("The operand for this operation."), MathTypesOrVar));

  ParamVar _operand{shards::Var(0)};
  ExposedInfo _requiredInfo{};
  OpType _opType = Invalid;
  SHType _dispatchType{SHType::None};  // Resolved type for direct dispatch

  void cleanup(SHContext *context) { _operand.cleanup(); }

  void warmup(SHContext *context) {
    _operand.warmup(context);
    if (_dispatchType != SHType::None) {
      _result = {};
      _result.valueType = _dispatchType;
    }
  }

  static SHParametersInfo parameters() { return SHParametersInfo(mathParamsInfo); }

  shards::Error formatTypeError(const SHType &inputType, const SHType &paramType) {
    std::stringstream errStream;
    errStream << "Operation not supported between different types ";
    errStream << "(input=" << type2Name(inputType);
    errStream << ", param=" << type2Name(paramType) << ")";
    return shards::Error(errStream.str());
  }

  OpType validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    OpType opType = OpType::Invalid;
    if (rhs != SHType::Seq && lhs.basicType == SHType::Seq) {
      if (lhs.seqTypes.len != 1)
        throw shards::Error(fmt::format("Operation not supported with input sequence with multiple types: {}", lhs));
      if (rhs != lhs.seqTypes.elements[0].basicType)
        throw formatTypeError(lhs.seqTypes.elements[0].basicType, rhs);

      opType = Seq1;
    } else if (rhs == SHType::Seq && lhs.basicType == SHType::Seq) {
      opType = SeqSeq;
    }
    return opType;
  }

  template <typename TValidator>
  SHTypeInfo genericCompose(TValidator &validator, const SHInstanceData &data, SHTypeInfo *operandType = nullptr) {
    SHTypeInfo resultType = data.inputType;
    SHVar operandSpec = _operand;
    if (operandSpec.valueType == SHType::ContextVar) {
      auto &ctx = CompositionContext::get(data);
      bool variableFound = false;

      auto varIt = ctx.inherited.find(SHSTRVIEW(operandSpec));
      if (varIt != ctx.inherited.end()) {
        _opType = validator.validateTypes(data.inputType, varIt->second.exposedType.basicType, resultType);
        variableFound = true;
        if (operandType) {
          *operandType = varIt->second.exposedType;
        }
      }
      if (!variableFound)
        throw shards::Error(fmt::format("Operand variable \"{}\" not found", SHSTRVIEW(operandSpec)));
    } else {
      _opType = validator.validateTypes(data.inputType, operandSpec.valueType, resultType);
      if (operandType) {
        operandType->basicType = operandSpec.valueType;
      }
    }

    if (_opType == Invalid) {
      throw shards::Error("Incompatible types for binary operation");
    }

    return resultType;
  }

  SHTypeInfo composeV2(const SHInstanceData &data) { return genericCompose(*this, data); }

  SHExposedTypesInfo requiredVariables() {
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

// =============================================================================
// Apply helpers (kept for compatibility with existing code)
// =============================================================================

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

template <typename TOp> struct ApplyUnary {
  template <SHType ValueType> void apply(SHVarPayload &out, const SHVarPayload &a) {
    typename PayloadTraits<ValueType>::ApplyUnary unary{};
    unary.template apply<TOp>(getPayloadContents<ValueType>(out), getPayloadContents<ValueType>(a));
  }
};

// =============================================================================
// OVERRIDE_ACTIVATE helpers for math operations
// =============================================================================

// Helper to select the right activate override based on type at compose time
// Returns true if override was set, false if fallback needed
// DT template parameter controls which types are valid (IntTypes, FloatTypes, NumberTypes, etc.)
template <typename TOp, typename TShard, DispatchType DT = DispatchType::NumberTypes>
bool overrideBinaryActivateForType(const SHInstanceData &data, SHType type, TShard *self) {
  // Lambda generator for binary ops with explicit shard type
  auto setActivate = [&data]<SHType VType>() {
    data.shard->activate = static_cast<SHActivateProc>([](Shard *b, SHContext *ctx, const SHVar *v) -> const SHVar * {
      auto wrapper = reinterpret_cast<shards::ShardWrapper<TShard> *>(b);
      try {
        wrapper->shard._result.valueType = VType;
        auto operand = wrapper->shard._operand.get();
        ::shards::Math::typedBinaryOp<TOp, VType>(wrapper->shard._result.payload, v->payload, operand.payload);
        return &wrapper->shard._result;
      } catch (std::exception &e) {
        shards::abortWire(ctx, e.what());
        return &wrapper->shard._result;
      }
    });
  };

  switch (type) {
  case SHType::Int:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int>();
      return true;
    }
    break;
  case SHType::Int2:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int2>();
      return true;
    }
    break;
  case SHType::Int3:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int3>();
      return true;
    }
    break;
  case SHType::Int4:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int4>();
      return true;
    }
    break;
  case SHType::Int8:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int8>();
      return true;
    }
    break;
  case SHType::Int16:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int16>();
      return true;
    }
    break;
  case SHType::Float:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes)) {
      setActivate.template operator()<SHType::Float>();
      return true;
    }
    break;
  case SHType::Float2:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes)) {
      setActivate.template operator()<SHType::Float2>();
      return true;
    }
    break;
  case SHType::Float3:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes)) {
      setActivate.template operator()<SHType::Float3>();
      return true;
    }
    break;
  case SHType::Float4:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes)) {
      setActivate.template operator()<SHType::Float4>();
      return true;
    }
    break;
  case SHType::Color:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Color>();
      return true;
    }
    break;
  case SHType::Bool:
    if constexpr (hasDispatchType(DT, DispatchType::BoolTypes)) {
      setActivate.template operator()<SHType::Bool>();
      return true;
    }
    break;
  case SHType::Audio:
    if constexpr (hasDispatchType(DT, DispatchType::AudioTypes)) {
      data.shard->activate = static_cast<SHActivateProc>([](Shard *b, SHContext *ctx, const SHVar *v) -> const SHVar * {
        auto wrapper = reinterpret_cast<shards::ShardWrapper<TShard> *>(b);
        try {
          auto operand = wrapper->shard._operand.get();
          const auto &inAudio = v->payload.audioValue;
          const auto &opAudio = operand.payload.audioValue;

          if (inAudio.nsamples != opAudio.nsamples || inAudio.channels != opAudio.channels) {
            throw ActivationError(fmt::format("Audio dimension mismatch: {}x{} vs {}x{}", inAudio.nsamples, inAudio.channels,
                                              opAudio.nsamples, opAudio.channels));
          }

          size_t total = size_t(inAudio.nsamples) * inAudio.channels;
          auto &result = wrapper->shard._result;
          size_t resultCapacity =
              result.valueType == SHType::Audio ? SHVAR_AUDIO_GET_CAPACITY(result) : 0;

          // Reallocate if needed (like cloneVar pattern)
          if (result.valueType != SHType::Audio || total > resultCapacity) {
            destroyVar(result);
            result.valueType = SHType::Audio;
            result.payload.audioValue.samples = new float[total];
            SHVAR_AUDIO_SET_CAPACITY(result, total);
          }

          applyBinaryAudioOp<TOp>(result.payload.audioValue.samples, inAudio.samples, opAudio.samples, total);

          result.payload.audioValue.nsamples = inAudio.nsamples;
          result.payload.audioValue.sampleRate = inAudio.sampleRate;
          result.payload.audioValue.channels = inAudio.channels;
          result.payload.audioValue.reserved = 0;
          return &result;
        } catch (std::exception &e) {
          shards::abortWire(ctx, e.what());
          return &wrapper->shard._result;
        }
      });
      return true;
    }
    break;
  default:
    break;
  }
  return false;
}

// Helper for audio + scalar binary operations
template <typename TOp, typename TShard, DispatchType DT = DispatchType::AudioTypes>
bool overrideBinaryActivateForAudioScalar(const SHInstanceData &data, TShard *self) {
  if constexpr (hasDispatchType(DT, DispatchType::AudioTypes)) {
    data.shard->activate = static_cast<SHActivateProc>([](Shard *b, SHContext *ctx, const SHVar *v) -> const SHVar * {
      auto wrapper = reinterpret_cast<shards::ShardWrapper<TShard> *>(b);
      try {
        auto operand = wrapper->shard._operand.get();
        const auto &inAudio = v->payload.audioValue;
        float scalar = float(operand.payload.floatValue);

        size_t total = size_t(inAudio.nsamples) * inAudio.channels;
        auto &result = wrapper->shard._result;
        size_t resultCapacity =
            result.valueType == SHType::Audio ? SHVAR_AUDIO_GET_CAPACITY(result) : 0;

        // Reallocate if needed (like cloneVar pattern)
        if (result.valueType != SHType::Audio || total > resultCapacity) {
          destroyVar(result);
          result.valueType = SHType::Audio;
          result.payload.audioValue.samples = new float[total];
          SHVAR_AUDIO_SET_CAPACITY(result, total);
        }

        applyBinaryAudioScalarOp<TOp>(result.payload.audioValue.samples, inAudio.samples, scalar, total);

        result.payload.audioValue.nsamples = inAudio.nsamples;
        result.payload.audioValue.sampleRate = inAudio.sampleRate;
        result.payload.audioValue.channels = inAudio.channels;
        result.payload.audioValue.reserved = 0;
        return &result;
      } catch (std::exception &e) {
        shards::abortWire(ctx, e.what());
        return &wrapper->shard._result;
      }
    });
    return true;
  }
  return false;
}

template <typename TOp, typename TShard, DispatchType DT = DispatchType::NumberTypes>
bool overrideUnaryActivateForType(const SHInstanceData &data, SHType type, TShard *self) {
  // Lambda generator for unary ops with explicit shard type
  auto setActivate = [&data]<SHType VType>() {
    data.shard->activate = static_cast<SHActivateProc>([](Shard *b, SHContext *ctx, const SHVar *v) -> const SHVar * {
      auto wrapper = reinterpret_cast<shards::ShardWrapper<TShard> *>(b);
      try {
        wrapper->shard._result.valueType = VType;
        ::shards::Math::typedUnaryOp<TOp, VType>(wrapper->shard._result.payload, v->payload);
        return &wrapper->shard._result;
      } catch (std::exception &e) {
        shards::abortWire(ctx, e.what());
        return &wrapper->shard._result;
      }
    });
  };

  switch (type) {
  case SHType::Int:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int>();
      return true;
    }
    break;
  case SHType::Int2:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int2>();
      return true;
    }
    break;
  case SHType::Int3:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int3>();
      return true;
    }
    break;
  case SHType::Int4:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int4>();
      return true;
    }
    break;
  case SHType::Int8:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int8>();
      return true;
    }
    break;
  case SHType::Int16:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Int16>();
      return true;
    }
    break;
  case SHType::Float:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes)) {
      setActivate.template operator()<SHType::Float>();
      return true;
    }
    break;
  case SHType::Float2:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes)) {
      setActivate.template operator()<SHType::Float2>();
      return true;
    }
    break;
  case SHType::Float3:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes)) {
      setActivate.template operator()<SHType::Float3>();
      return true;
    }
    break;
  case SHType::Float4:
    if constexpr (hasDispatchType(DT, DispatchType::FloatTypes)) {
      setActivate.template operator()<SHType::Float4>();
      return true;
    }
    break;
  case SHType::Color:
    if constexpr (hasDispatchType(DT, DispatchType::IntTypes)) {
      setActivate.template operator()<SHType::Color>();
      return true;
    }
    break;
  case SHType::Audio:
    if constexpr (hasDispatchType(DT, DispatchType::AudioTypes)) {
      data.shard->activate = static_cast<SHActivateProc>([](Shard *b, SHContext *ctx, const SHVar *v) -> const SHVar * {
        auto wrapper = reinterpret_cast<shards::ShardWrapper<TShard> *>(b);
        try {
          const auto &inAudio = v->payload.audioValue;
          size_t total = size_t(inAudio.nsamples) * inAudio.channels;
          auto &result = wrapper->shard._result;
          size_t resultCapacity =
              result.valueType == SHType::Audio ? SHVAR_AUDIO_GET_CAPACITY(result) : 0;

          // Reallocate if needed (like cloneVar pattern)
          if (result.valueType != SHType::Audio || total > resultCapacity) {
            destroyVar(result);
            result.valueType = SHType::Audio;
            result.payload.audioValue.samples = new float[total];
            SHVAR_AUDIO_SET_CAPACITY(result, total);
          }

          applyUnaryAudioOp<TOp>(result.payload.audioValue.samples, inAudio.samples, total);

          result.payload.audioValue.nsamples = inAudio.nsamples;
          result.payload.audioValue.sampleRate = inAudio.sampleRate;
          result.payload.audioValue.channels = inAudio.channels;
          result.payload.audioValue.reserved = 0;
          return &result;
        } catch (std::exception &e) {
          shards::abortWire(ctx, e.what());
          return &wrapper->shard._result;
        }
      });
      return true;
    }
    break;
  // Note: No unary operations currently use BoolTypes
  default:
    break;
  }
  return false;
}

} // namespace Math
} // namespace shards

#endif // SH_CORE_SHARDS_MATH_BASE

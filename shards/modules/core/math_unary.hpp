/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_SHARDS_MATH_UNARY
#define SH_CORE_SHARDS_MATH_UNARY

#include "math_base.hpp"

#define _PC
#include "ShaderFastMathLib.h"
#undef _PC

namespace shards {
namespace Math {

// =============================================================================
// Basic Unary Operation - Template for unary ops with type dispatch
// =============================================================================

template <typename TOp, DispatchType DispatchType_ = DispatchType::NumberTypes> 
struct BasicUnaryOperation {
  static constexpr DispatchType DispatchType__ = DispatchType_;
  using OpType_ = TOp;  // Expose the operation type
  
  ApplyUnary<TOp> apply;

  OpType validateTypes(const SHTypeInfo &a, SHTypeInfo &resultType) {
    OpType opType = OpType::Invalid;
    return opType;
  }

  void operateDirect(SHVar &output, const SHVar &a, bool *failed) {
    output.valueType = a.valueType;
    dispatchType<DispatchType_>(a.valueType, failed, apply, output.payload, a.payload);
  }
};

// =============================================================================
// Unary Operation - Main template for unary operations
// =============================================================================

// Type trait to detect if TOp has OpType_ and DispatchType__ (i.e., is a BasicUnaryOperation)
template <typename T, typename = void>
struct HasUnaryOpTraits : std::false_type {};

template <typename T>
struct HasUnaryOpTraits<T, std::void_t<typename T::OpType_, decltype(T::DispatchType__)>> : std::true_type {};

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
    
    // Use OVERRIDE_ACTIVATE for direct operations (only if TOp supports it)
    if constexpr (HasUnaryOpTraits<TOp>::value) {
      if (_opType == OpType::Direct) {
        _dispatchType = data.inputType.basicType;
        using ThisType = std::remove_pointer_t<decltype(this)>;
        overrideUnaryActivateForType<typename TOp::OpType_, ThisType, TOp::DispatchType__>(
            data, data.inputType.basicType, this);
      }
    }
    return resultType;
  }

  static SHOptionalString help() {
    return SHCCSTR("Applies the unary operation on the input value and outputs the result. If the input is a sequence, the "
                   "operation is applied to each element of the sequence.");
  }

  NO_INLINE void cancelFlow(SHContext *context, const SHVar &input, const SHVar &a) {
    context->cancelFlow(fmt::format("Invalid types for unary operation: {} and {}", magic_enum::enum_name(a.valueType),
                                    magic_enum::enum_name(input.valueType)));
  }

  ALWAYS_INLINE void operate(SHVar &output, const SHVar &a, bool *failed) {
    if (likely(_opType == OpType::Direct)) {
      op.operateDirect(output, a, failed);
    } else if (_opType == OpType::Seq1) {
      if (output.valueType != SHType::Seq) {
        destroyVar(output);
        output.valueType = SHType::Seq;
      }

      shards::arrayResize(output.payload.seqValue, a.payload.seqValue.len);
      for (uint32_t i = 0; i < a.payload.seqValue.len; i++) {
        op.operateDirect(output.payload.seqValue.elements[i], a.payload.seqValue.elements[i], failed);
      }
    } else {
      *failed = true;
    }
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    bool failed = false;
    operate(_result, input, &failed);
    if (failed) {
      cancelFlow(context, input, _result);
    }
    return _result;
  }
};

// =============================================================================
// Specialized Unary Operations
// =============================================================================

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

template <class TOp> struct UnaryIntOperation : public UnaryOperation<TOp> {
  static inline Types IntOrSeqTypes{{CoreInfo::IntType, CoreInfo::Int2Type, CoreInfo::Int3Type, CoreInfo::Int4Type,
                                     CoreInfo::Int8Type, CoreInfo::Int16Type, CoreInfo::AnySeqType}};

  static SHTypesInfo inputTypes() { return IntOrSeqTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("An integer, a vector of integers (Int2, Int3, Int4), a sequence of these types supported by this operation.");
  }
  static SHTypesInfo outputTypes() { return IntOrSeqTypes; }
};

// =============================================================================
// Unary Operation Macros
// =============================================================================

#define MATH_UNARY_OPERATION(NAME, FUNCI, FUNCF)    \
  struct NAME##Op final {                           \
    template <typename T> T apply(const T &lhs) {   \
      if constexpr (std::is_unsigned_v<T>) {        \
        return lhs;                                 \
      } else if constexpr (std::is_integral_v<T>) { \
        return FUNCI(lhs);                          \
      } else {                                      \
        return FUNCF(lhs);                          \
      }                                             \
    }                                               \
  };

#define MATH_UNARY_FLOAT_OPERATION(NAME, FUNC, FUNCF)                 \
  struct NAME##Op final {                                             \
    template <typename T> T apply(const T &lhs) { return FUNC(lhs); } \
  };

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

// =============================================================================
// Concrete Unary Operations
// =============================================================================

struct Abs : public UnaryOperation<BasicUnaryOperation<AbsOp, DispatchType::NumberTypes>> {
  static SHOptionalString help() { return SHCCSTR("This shard outputs the absolute value of the input."); }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The numeric value or a sequence of numeric values to get the absolute value of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the absolute value of the input."); }
};

struct Exp : public UnaryFloatOperation<BasicUnaryOperation<ExpOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the exponential function with base e (Euler's number) for the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to use as the exponent for the base e exponential function.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the result of the exponential operation."); }
};

struct Exp2 : public UnaryFloatOperation<BasicUnaryOperation<Exp2Op, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the exponential function with base 2 for the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats used as the exponent for the base 2 exponential function.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the result of the exponential operation."); }
};

struct Expm1 : public UnaryFloatOperation<BasicUnaryOperation<Expm1Op, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the exponential function with base e (Euler's number) for the given input and "
                   "subtracts 1 from the result.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats used as the exponent for the base e exponential function.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the result of the exponential operation."); }
};

struct Log : public UnaryFloatOperation<BasicUnaryOperation<LogOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the natural logarithm for the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the natural logarithm of. Must be positive.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the natural logarithm of the input."); }
};

struct Log10 : public UnaryFloatOperation<BasicUnaryOperation<Log10Op, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the base 10 logarithm for the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the base 10 logarithm of. Must be positive.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the base 10 logarithm of the input."); }
};

struct Log2 : public UnaryFloatOperation<BasicUnaryOperation<Log2Op, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the base 2 logarithm for the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the base 2 logarithm of. Must be positive.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the base 2 logarithm."); }
};

struct Log1p : public UnaryFloatOperation<BasicUnaryOperation<Log1pOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard adds 1 to the input and then calculates the natural logarithm of the result.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to add 1 to and then calculate the natural logarithm of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the natural logarithm of the input plus 1."); }
};

struct Sqrt : public UnaryFloatOperation<BasicUnaryOperation<SqrtOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() { return SHCCSTR("This shard calculates the square root of the given input."); }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the square root of. Must be positive.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the square root of the input."); }
};

struct FastSqrt : public UnaryFloatOperation<BasicUnaryOperation<FastSqrtOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() { return SHCCSTR("This shard calculates the square root of the given input (fast approximation)."); }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the square root of. Must be positive.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the square root of the input."); }
};

struct FastInvSqrt : public UnaryFloatOperation<BasicUnaryOperation<FastInvSqrtOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() { return SHCCSTR("This shard calculates the inverse square root of the given input (fast approximation)."); }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the inverse square root of. Must be positive.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the inverse square root of the input."); }
};

struct Cbrt : public UnaryFloatOperation<BasicUnaryOperation<CbrtOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() { return SHCCSTR("This shard calculates the cube root of the given input."); }

  static SHOptionalString inputHelp() { return SHCCSTR("The input float or sequence of floats to calculate the cube root of."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the cube root of the input."); }
};

struct Sin : public UnaryFloatOperation<BasicUnaryOperation<SinOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the sine of the given input, where the input is the angle in radians.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The input float or sequence of floats to calculate the sine of."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the sine of the input."); }
};

struct Cos : public UnaryFloatOperation<BasicUnaryOperation<CosOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the cosine of the given input, where the input is the angle in radians.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The input float or sequence of floats to calculate the cosine of."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the cosine of the input."); }
};

struct Tan : public UnaryFloatOperation<BasicUnaryOperation<TanOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the tangent of the given input, where the input is the angle in radians.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The input float or sequence of floats to calculate the tangent of."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the tangent of the input."); }
};

struct Asin : public UnaryFloatOperation<BasicUnaryOperation<AsinOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the inverse sine of the given input (arc sine).");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the inverse sine of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the angle in radians whose sine is the input value."); }
};

struct Acos : public UnaryFloatOperation<BasicUnaryOperation<AcosOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the inverse cosine of the given input (arc cosine).");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the inverse cosine of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the angle in radians whose cosine is the input value."); }
};

struct Atan : public UnaryFloatOperation<BasicUnaryOperation<AtanOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the inverse tangent of the given input (arc tangent).");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the inverse tangent of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the angle in radians whose tangent is the input value."); }
};

struct Sinh : public UnaryFloatOperation<BasicUnaryOperation<SinhOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the hyperbolic sine of the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the hyperbolic sine of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the hyperbolic sine of the input."); }
};

struct Cosh : public UnaryFloatOperation<BasicUnaryOperation<CoshOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the hyperbolic cosine of the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the hyperbolic cosine of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the hyperbolic cosine of the input."); }
};

struct Tanh : public UnaryFloatOperation<BasicUnaryOperation<TanhOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the hyperbolic tangent of the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the hyperbolic tangent of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the hyperbolic tangent of the input."); }
};

struct Asinh : public UnaryFloatOperation<BasicUnaryOperation<AsinhOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the inverse hyperbolic sine of the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the inverse hyperbolic sine of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the real number whose hyperbolic sine is the input value."); }
};

struct Acosh : public UnaryFloatOperation<BasicUnaryOperation<AcoshOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the inverse hyperbolic cosine of the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the inverse hyperbolic cosine of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the real number whose hyperbolic cosine is the input value."); }
};

struct Atanh : public UnaryFloatOperation<BasicUnaryOperation<AtanhOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the inverse hyperbolic tangent of the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the inverse hyperbolic tangent of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the real number whose hyperbolic tangent is the input value."); }
};

struct Erf : public UnaryFloatOperation<BasicUnaryOperation<ErfOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the error function of the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the error function of.");
  }

  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs probability result of the error function. Output is always between -1 and 1.");
  }
};

struct Erfc : public UnaryFloatOperation<BasicUnaryOperation<ErfcOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the complementary error function of the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the complementary error function of.");
  }

  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs the probability result of the complementary error function. Output is always between 0 and 2.");
  }
};

struct TGamma : public UnaryFloatOperation<BasicUnaryOperation<TGammaOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the gamma function of the given input.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The input float or sequence of floats to calculate the gamma function of."); }

  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs the gamma function of the input. Always positive for positive inputs.");
  }
};

struct LGamma : public UnaryFloatOperation<BasicUnaryOperation<LGammaOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the log gamma function of the given input.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The input float or sequence of floats to calculate the log gamma function of.");
  }

  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs the log gamma function of the input. Always positive for positive inputs.");
  }
};

struct Ceil : public UnaryFloatOperation<BasicUnaryOperation<CeilOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() { return SHCCSTR("This shard rounds up the input to the nearest integer."); }

  static SHOptionalString inputHelp() { return SHCCSTR("The input float or sequence of floats to round up."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the input rounded up to the nearest integer (as a float)."); }
};

struct Floor : public UnaryFloatOperation<BasicUnaryOperation<FloorOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() { return SHCCSTR("This shard rounds down the input to the nearest integer."); }

  static SHOptionalString inputHelp() { return SHCCSTR("The input float or sequence of floats to round down."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the input rounded down to the nearest integer (as a float)."); }
};

struct Trunc : public UnaryFloatOperation<BasicUnaryOperation<TruncOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard truncates the input towards zero, removing any fractional part without rounding.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The input float or sequence of floats to truncate."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the input truncated to the nearest integer (as a float)."); }
};

struct Round : public UnaryFloatOperation<BasicUnaryOperation<RoundOp, DispatchType::FloatTypes>> {
  static SHOptionalString help() { return SHCCSTR("This shard rounds the input to the nearest integer."); }

  static SHOptionalString inputHelp() { return SHCCSTR("The input float or sequence of floats to round."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the input rounded to the nearest integer (as a float)."); }
};

// =============================================================================
// Negate and Not operations
// =============================================================================

struct NegateOp {
  template <typename T> T apply(const T &a) { return -a; }
};

struct Negate : public UnaryOperation<BasicUnaryOperation<NegateOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard reverses the sign of the input. (A positive number becomes negative, and vice versa).");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The float or integer (or sequence of floats or integers) to reverse the sign of.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("The input with its sign reversed."); }
};

struct NotOp {
  template <typename T> T apply(const T &a) { return ~a; }
};

struct Not : public UnaryIntOperation<BasicUnaryOperation<NotOp, DispatchType::IntTypes>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard performs a bitwise NOT operation on the input. It flips all the bits of the input number.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The integer (or sequence of integers) to perform bitwise NOT on."); }

  static SHOptionalString outputHelp() { return SHCCSTR("The result of the bitwise NOT operation."); }
};

} // namespace Math
} // namespace shards

#endif // SH_CORE_SHARDS_MATH_UNARY

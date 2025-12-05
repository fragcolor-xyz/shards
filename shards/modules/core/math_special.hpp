/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_SHARDS_MATH_SPECIAL
#define SH_CORE_SHARDS_MATH_SPECIAL

#include "math_base.hpp"
#include "math_unary.hpp"
#include "math_binary.hpp"

namespace shards {
namespace Math {

// =============================================================================
// Inc/Dec operations - Variable modifiers
// =============================================================================

template <class TOp> struct UnaryVarOperation : public UnaryOperation<TOp> {
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
      throw shards::Error("UnaryVarOperation Expected a variable");
    }

    for (const auto &share : data.shared) {
      if (share.name == SHSTRVIEW((*_value))) {
        if (share.isProtected)
          throw shards::Error("UnaryVarOperation cannot write protected variables");
        if (!share.isMutable)
          throw shards::Error("UnaryVarOperation attempt to write immutable variable");

        this->validateTypes(share.exposedType, resultType);
        assert(resultType == data.inputType);

        return resultType;
      }
    }

    throw shards::Error(fmt::format("Math.Inc/Dec variable {} not found", SHSTRVIEW((*_value))));
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

  NO_INLINE void cancelFlow(SHContext *context, const SHVar &input, const SHVar &a) {
    context->cancelFlow(fmt::format("Invalid types for unary operation: {} and {}", magic_enum::enum_name(a.valueType),
                                    magic_enum::enum_name(input.valueType)));
  }

  ALWAYS_INLINE void activate(SHContext *context, const SHVar &input) {
    bool failed = false;
    this->operate(_value.get(), _value.get(), &failed);
    if (failed) {
      cancelFlow(context, input, _value.get());
    }
  }
};

struct IncOp {
  template <typename T> T apply(const T &a) { return a + 1; }
};

struct Inc : public UnaryVarOperation<BasicUnaryOperation<IncOp>> {
  static SHOptionalString help() { return SHCCSTR("Increases the input by 1."); }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The float or integer (or sequence of floats or integers) to increase by 1.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("The input increased by 1."); }
};

struct DecOp {
  template <typename T> T apply(const T &a) { return a - 1; }
};

struct Dec : public UnaryVarOperation<BasicUnaryOperation<DecOp>> {
  static SHOptionalString help() { return SHCCSTR("Decreases the input by 1."); }

  static SHOptionalString inputHelp() {
    return SHCCSTR("The float or integer (or sequence of floats or integers) to decrease by 1.");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("The input decreased by 1."); }
};

// =============================================================================
// Mean operation
// =============================================================================

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
  DECL_ENUM_INFO(
      MeanKind, Mean,
      "Type of mean calculation to be performed. Specifies whether to use arithmetic, geometric, or harmonic averaging.", 'mean');

  static SHOptionalString help() { return SHCCSTR("Calculates the average value of a sequence of floating point numbers."); }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatSeqType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The sequence of floating point numbers to calculate the average of."); }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The calculated average as a float."); }

  static SHParametersInfo parameters() {
    static Parameters params{{"Kind", SHCCSTR("The type of average to calculate."), {MeanEnumInfo::Type}}};
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

// =============================================================================
// Lerp operation
// =============================================================================

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

  static SHOptionalString inputHelp() { return SHCCSTR("The factor to interpolate between the start and end values."); }

  static SHOptionalString outputHelp() {
    return SHCCSTR("The interpolated value between the start and end values based on the factor provided as input.");
  }

  static SHOptionalString help() {
    return SHCCSTR("Linearly interpolate between the start value specified in the `First` parameter and the end value specified "
                   "in the `Second` parameter based on the factor provided as input.");
  }

  static SHParametersInfo parameters() {
    static Parameters params{
        {"First", SHCCSTR("The start value"), BinaryBase::OnlyNumbers},
        {"Second", SHCCSTR("The end value"), BinaryBase::OnlyNumbers},
    };
    return params;
  }

  ParamVar _first;
  ParamVar _second;
  Var _result;
  ExposedInfo _required;

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

  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo(_required); }

  void cleanup(SHContext *context) {
    _first.cleanup();
    _second.cleanup();
  }

  SHTypeInfo composeV2(SHInstanceData &data) {
    SHType firstType{};
    SHType secondType{};
    firstType = _first.isVariable() ? findParamVarExposedTypeChecked(data, _first).exposedType.basicType : _first->valueType;
    secondType = _second.isVariable() ? findParamVarExposedTypeChecked(data, _second).exposedType.basicType : _second->valueType;

    collectRequiredVariables(data, _required, (SHVar &)_first);
    collectRequiredVariables(data, _required, (SHVar &)_second);

    if (firstType != secondType)
      throw shards::Error("Types should match");

    return SHTypeInfo{.basicType = firstType};
  }

  NO_INLINE void cancelFlow(SHContext *context, const SHVar &a, const SHVar &b) {
    context->cancelFlow(fmt::format("Invalid types for ApplyLerp: {} and {}", magic_enum::enum_name(a.valueType),
                                    magic_enum::enum_name(b.valueType)));
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    SHVar a = _first.get();
    SHVar b = _second.get();
    SHVar result{.valueType = a.valueType};
    bool failed = false;
    dispatchType<DispatchType::NumberTypes>(a.valueType, &failed, ApplyLerp{}, result.payload, a.payload, b.payload,
                                            input.payload.floatValue);
    if (failed) {
      cancelFlow(context, a, b);
    }
    return result;
  }
};

// =============================================================================
// Clamp operation
// =============================================================================

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

  static SHOptionalString help() {
    return SHCCSTR("This shard ensures the input value falls within the specified range. If the value falls below the minimum, "
                   "the Min value is returned. If the value exceeds the maximum, the Max value is returned. Otherwise, the value "
                   "is returned unchanged.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The value to clamp."); }

  static SHOptionalString outputHelp() { return SHCCSTR("The clamped value."); }

  static SHParametersInfo parameters() {
    static Parameters params{
        {"Min", SHCCSTR("The lower bound of the range"), BinaryBase::MathTypesOrVar},
        {"Max", SHCCSTR("The upper bound of the range"), BinaryBase::MathTypesOrVar},
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
      throw shards::Error("Types should match");

    return SHTypeInfo{.basicType = firstType};
  }

  NO_INLINE void cancelFlow(SHContext *context, const SHVar &a, const SHVar &b) {
    context->cancelFlow(fmt::format("Invalid types for ApplyClamp: {} and {}", magic_enum::enum_name(a.valueType),
                                    magic_enum::enum_name(b.valueType)));
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    SHVar a = _first.get();
    SHVar b = _second.get();
    SHVar result{.valueType = a.valueType};
    bool failed = false;
    dispatchType<DispatchType::NumberTypes>(a.valueType, &failed, ApplyClamp{}, result.payload, input.payload, a.payload,
                                            b.payload);
    if (failed) {
      cancelFlow(context, a, b);
    }
    return result;
  }
};

// =============================================================================
// Percentile operation
// =============================================================================

struct Percentile {
  Percentile() { _percentile = Var{50.0}; }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatSeqType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  static SHOptionalString help() {
    return SHCCSTR("This shard calculates the percentile of the input value within the specified sequence.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("The sequence of floats to calculate the percentile of."); }

  static SHOptionalString outputHelp() { return SHCCSTR("The percentile of the input value within the specified sequence."); }

  PARAM_PARAMVAR(_percentile, "Percentile", "The percentile to calculate.", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_percentile));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  std::vector<double> _sortedList;

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    auto percentile = _percentile.get().payload.floatValue / 100.0;

    _sortedList.resize(input.payload.seqValue.len);
    for (size_t i = 0; i < input.payload.seqValue.len; ++i) {
      _sortedList[i] = input.payload.seqValue.elements[i].payload.floatValue;
    }

    std::sort(_sortedList.begin(), _sortedList.end());

    auto index = percentile * (_sortedList.size() - 1);

    // index is not an integer so we interpolate between the lower and upper bounds
    auto lower = _sortedList[static_cast<int>(std::floor(index))];
    auto upper = _sortedList[static_cast<int>(std::ceil(index))];

    auto threshold = lower + (upper - lower) * (index - std::floor(index));

    return Var{threshold};
  }
};

} // namespace Math
} // namespace shards

#endif // SH_CORE_SHARDS_MATH_SPECIAL

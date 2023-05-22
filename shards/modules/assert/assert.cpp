/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>

namespace shards {
namespace Assert {

struct Base {
  static inline ParamsInfo assertParamsInfo =
      ParamsInfo(ParamsInfo::Param("Value", SHCCSTR("The value to test against for equality."), CoreInfo::AnyType),
                 ParamsInfo::Param("Abort", SHCCSTR("If we should abort the process on failure."), CoreInfo::BoolType));

  SHVar value{};
  bool aborting;

  void destroy() { destroyVar(value); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input can be of any type."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output will be the input (passthrough)."); }

  SHParametersInfo parameters() { return SHParametersInfo(assertParamsInfo); }

  void setParam(int index, const SHVar &inValue) {
    switch (index) {
    case 0:
      destroyVar(value);
      cloneVar(value, inValue);
      break;
    case 1:
      aborting = inValue.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    auto res = SHVar();
    switch (index) {
    case 0:
      res = value;
      break;
    case 1:
      res.valueType = SHType::Bool;
      res.payload.boolValue = aborting;
      break;
    default:
      break;
    }
    return res;
  }
};

struct Is : public Base {
  SHOptionalString help() {
    return SHCCSTR("This assertion is used to check whether the input is equal "
                   "to a given value.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (input != value) {
      SHLOG_ERROR("Failed assertion Is, input: {} expected: {}", input, value);
      if (aborting)
        abort();
      else
        throw ActivationError("Assert failed - Is");
    }

    return input;
  }
};

struct IsNot : public Base {
  SHOptionalString help() {
    return SHCCSTR("This assertion is used to check whether the input is "
                   "different from a given value.");
  }
  SHVar activate(SHContext *context, const SHVar &input) {
    if (input == value) {
      SHLOG_ERROR("Failed assertion IsNot, input: {} not expected: {}", input, value);
      if (aborting)
        abort();
      else
        throw ActivationError("Assert failed - IsNot");
    }

    return input;
  }
};

struct IsAlmost {
  SHOptionalString help() {
    return SHCCSTR("This assertion is used to check whether the input is almost equal to a given value.");
  }

  static SHTypesInfo inputTypes() { return MathTypes; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input can be of any number type or a sequence of such types."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output will be the input (passthrough)."); }

  SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &inValue) {
    switch (index) {
    case 0:
      destroyVar(_value);
      cloneVar(_value, inValue);
      break;
    case 1:
      _aborting = inValue.payload.boolValue;
      break;
    case 2:
      if (inValue.valueType == SHType::Float)
        _threshold = inValue.payload.floatValue;
      else
        _threshold = double(inValue.payload.intValue);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _value;
    case 1:
      return Var(_aborting);
    case 2:
      return Var(_threshold);
    default:
      return Var::Empty;
    }
  }

  void destroy() { destroyVar(_value); }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (_value.valueType != data.inputType.basicType)
      throw SHException("Input and value types must match.");

    if (_threshold <= 0.0)
      throw SHException("Threshold must be greater than 0.");

    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!_almostEqual(input, _value, _threshold)) {
      SHLOG_ERROR("Failed assertion IsAlmost, input: {} expected: {}", input, _value);
      if (_aborting)
        abort();
      else
        throw ActivationError("Assert failed - IsAlmost");
    }

    return input;
  }

private:
  static inline Types MathTypes{{
      CoreInfo::FloatType,
      CoreInfo::Float2Type,
      CoreInfo::Float3Type,
      CoreInfo::Float4Type,
      CoreInfo::IntType,
      CoreInfo::Int2Type,
      CoreInfo::Int3Type,
      CoreInfo::Int4Type,
      CoreInfo::Int8Type,
      CoreInfo::Int16Type,
      CoreInfo::AnySeqType,
  }};
  static inline Parameters _params = {{"Value", SHCCSTR("The value to test against for almost equality."), MathTypes},
                                      {"Abort", SHCCSTR("If we should abort the process on failure."), {CoreInfo::BoolType}},
                                      {"Threshold",
                                       SHCCSTR("The smallest difference to be considered equal. Should be greater than zero."),
                                       {CoreInfo::FloatType, CoreInfo::IntType}}};

  bool _aborting;
  SHFloat _threshold{FLT_EPSILON};
  SHVar _value{};
};
} // namespace Assert
} // namespace shards

SHARDS_REGISTER_FN(assert) {
  using namespace shards::Assert;
  REGISTER_SHARD("Assert.Is", Is);
  REGISTER_SHARD("Assert.IsNot", IsNot);
  REGISTER_SHARD("Assert.IsAlmost", IsAlmost);
}

/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shared.hpp"

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
  static SHOptionalString inputHelp() {
    return SHCCSTR("The input can be of any decimal type or a sequence of such decimal type.");
  }

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
      _threshold = inValue.payload.floatValue;
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

    if (_threshold <= 0.0 || _threshold >= 1.0)
      throw SHException("Threshold must be stricly between 0 and 1.");

    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!isAlmost(input, _value)) {
      SHLOG_ERROR("Failed assertion IsAlmost, input: {} expected: {}", input, _value);
      if (_aborting)
        abort();
      else
        throw ActivationError("Assert failed - IsAlmost");
    }

    return input;
  }

private:
  bool isAlmost(const SHVar &lhs, const SHVar &rhs) {
    if (lhs.valueType != rhs.valueType) {
      return false;
    }

    switch (lhs.valueType) {
    case SHType::Float:
      return isAlmost(lhs.payload.floatValue, rhs.payload.floatValue);
    case SHType::Float2:
      return (isAlmost(lhs.payload.float2Value[0], rhs.payload.float2Value[0]) &&
              isAlmost(lhs.payload.float2Value[1], rhs.payload.float2Value[1]));
    case SHType::Float3:
      return (isAlmost(lhs.payload.float3Value[0], rhs.payload.float3Value[0]) &&
              isAlmost(lhs.payload.float3Value[1], rhs.payload.float3Value[1]) &&
              isAlmost(lhs.payload.float3Value[2], rhs.payload.float3Value[2]));
    case SHType::Float4:
      return (isAlmost(lhs.payload.float4Value[0], rhs.payload.float4Value[0]) &&
              isAlmost(lhs.payload.float4Value[1], rhs.payload.float4Value[1]) &&
              isAlmost(lhs.payload.float4Value[2], rhs.payload.float4Value[2]) &&
              isAlmost(lhs.payload.float4Value[3], rhs.payload.float4Value[3]));
    case SHType::Seq: {
      if (lhs.payload.seqValue.len != rhs.payload.seqValue.len) {
        return false;
      }

      auto almost = true;
      for (uint32_t i = 0; i < lhs.payload.seqValue.len; i++) {
        auto &suba = lhs.payload.seqValue.elements[i];
        auto &subb = rhs.payload.seqValue.elements[i];
        almost = almost && isAlmost(suba, subb);
      }

      return almost;
    }
    default:
      return lhs == rhs;
    }
  }

  bool isAlmost(double lhs, double rhs) { return __builtin_fabs(lhs - rhs) <= _threshold; }

  static inline Types MathTypes{{
      CoreInfo::FloatType,
      CoreInfo::Float2Type,
      CoreInfo::Float3Type,
      CoreInfo::Float4Type,
      CoreInfo::AnySeqType,
  }};
  static inline Parameters _params = {
      {"Value", SHCCSTR("The value to test against for almost equality."), MathTypes},
      {"Abort", SHCCSTR("If we should abort the process on failure."), {CoreInfo::BoolType}},
      {"Threshold", SHCCSTR("The smallest difference to be considered equal. Should be stricly in the ]0, 1[ range."), {CoreInfo::FloatType}}};

  bool _aborting;
  SHFloat _threshold{FLT_EPSILON};
  SHVar _value{};
};

void registerShards() {
  REGISTER_SHARD("Assert.Is", Is);
  REGISTER_SHARD("Assert.IsNot", IsNot);
  REGISTER_SHARD("Assert.IsAlmost", IsAlmost);
}
}; // namespace Assert
}; // namespace shards

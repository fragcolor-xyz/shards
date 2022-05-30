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
      res.valueType = Bool;
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

void registerShards() {
  REGISTER_SHARD("Assert.Is", Is);
  REGISTER_SHARD("Assert.IsNot", IsNot);
}
}; // namespace Assert
}; // namespace shards

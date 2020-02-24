/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "shared.hpp"

namespace chainblocks {
namespace Assert {

struct Base {
  static inline ParamsInfo assertParamsInfo = ParamsInfo(
      ParamsInfo::Param("Value", "The value to test against for equality.",
                        CoreInfo::AnyType),
      ParamsInfo::Param("Abort", "If we should abort the process on failure.",
                        CoreInfo::BoolType));

  CBVar value{};
  bool aborting;

  void destroy() { destroyVar(value); }

  CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBParametersInfo parameters() { return CBParametersInfo(assertParamsInfo); }

  void setParam(int index, CBVar inValue) {
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

  CBVar getParam(int index) {
    auto res = CBVar();
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
  CBVar activate(CBContext *context, const CBVar &input) {
    if (input != value) {
      LOG(ERROR) << "Failed assertion Is, input: " << input
                 << " expected: " << value;
      if (aborting)
        abort();
      else
        throw CBException("Assert failed - Is");
    }

    return input;
  }
};

struct IsNot : public Base {
  CBVar activate(CBContext *context, const CBVar &input) {
    if (input == value) {
      LOG(ERROR) << "Failed assertion IsNot, input: " << input
                 << " not expected: " << value;
      if (aborting)
        abort();
      else
        throw CBException("Assert failed - IsNot");
    }

    return input;
  }
};

DECLARE_CBLOCK("Assert.Is", Is);
DECLARE_CBLOCK("Assert.IsNot", IsNot);
void registerBlocks() {
  REGISTER_CBLOCK(Is);
  REGISTER_CBLOCK(IsNot);
}
}; // namespace Assert
}; // namespace chainblocks

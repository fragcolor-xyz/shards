#include "shared.hpp"

namespace chainblocks {
namespace Assert {

struct Base {
  static inline ParamsInfo assertParamsInfo = ParamsInfo(
      ParamsInfo::Param("Value", "The value to test against for equality.",
                        CBTypesInfo(SharedTypes::anyInfo)),
      ParamsInfo::Param("Abort", "If we should abort the process on failure.",
                        CBTypesInfo(SharedTypes::boolInfo)));

  CBVar value;
  bool aborting;

  void destroy() { destroyVar(value); }

  CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

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
  CBVar activate(CBContext *context, CBVar input) {
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
  CBVar activate(CBContext *context, CBVar input) {
    if (input == value) {
      LOG(ERROR) << "Failed assertion IsNot, input: " << input
                 << " expected: " << value;
      if (aborting)
        abort();
      else
        throw CBException("Assert failed - IsNot");
    }

    return input;
  }
};

// Register Is
RUNTIME_BLOCK(Assert, Is);
RUNTIME_BLOCK_destroy(Is);
RUNTIME_BLOCK_inputTypes(Is);
RUNTIME_BLOCK_outputTypes(Is);
RUNTIME_BLOCK_parameters(Is);
RUNTIME_BLOCK_setParam(Is);
RUNTIME_BLOCK_getParam(Is);
RUNTIME_BLOCK_activate(Is);
RUNTIME_BLOCK_END(Is);

// Register IsNot
RUNTIME_BLOCK(Assert, IsNot);
RUNTIME_BLOCK_destroy(IsNot);
RUNTIME_BLOCK_inputTypes(IsNot);
RUNTIME_BLOCK_outputTypes(IsNot);
RUNTIME_BLOCK_parameters(IsNot);
RUNTIME_BLOCK_setParam(IsNot);
RUNTIME_BLOCK_getParam(IsNot);
RUNTIME_BLOCK_activate(IsNot);
RUNTIME_BLOCK_END(IsNot);
}; // namespace Assert

void registerAssertBlocks() {
  REGISTER_BLOCK(Assert, Is);
  REGISTER_BLOCK(Assert, IsNot);
}
}; // namespace chainblocks

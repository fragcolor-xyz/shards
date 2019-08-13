#include "shared.hpp"

namespace chainblocks {
static ParamsInfo compareParamsInfo = ParamsInfo(
    ParamsInfo::Param("Value", "The value to test against for equality.",
                      CBTypesInfo(SharedTypes::anyInfo)));

struct BaseOpsBin {
  CBVar value{};

  void destroy() { destroyVar(value); }

  CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBParametersInfo parameters() { return CBParametersInfo(compareParamsInfo); }

  void setParam(int index, CBVar inValue) {
    switch (index) {
    case 0:
      destroyVar(value);
      cloneVar(value, inValue);
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
    default:
      break;
    }
    return res;
  }
};

struct Is : public BaseOpsBin {
  CBVar activate(CBContext *context, CBVar input) {
    if (input != value) {
      return False;
    }
    return True;
  }
};

struct IsNot : public BaseOpsBin {
  CBVar activate(CBContext *context, CBVar input) {
    if (input == value) {
      return False;
    }
    return True;
  }
};

// Register Is
RUNTIME_CORE_BLOCK(Is);
RUNTIME_BLOCK_destroy(Is);
RUNTIME_BLOCK_inputTypes(Is);
RUNTIME_BLOCK_outputTypes(Is);
RUNTIME_BLOCK_parameters(Is);
RUNTIME_BLOCK_setParam(Is);
RUNTIME_BLOCK_getParam(Is);
RUNTIME_BLOCK_activate(Is);
RUNTIME_BLOCK_END(Is);

// Register IsNot
RUNTIME_CORE_BLOCK(IsNot);
RUNTIME_BLOCK_destroy(IsNot);
RUNTIME_BLOCK_inputTypes(IsNot);
RUNTIME_BLOCK_outputTypes(IsNot);
RUNTIME_BLOCK_parameters(IsNot);
RUNTIME_BLOCK_setParam(IsNot);
RUNTIME_BLOCK_getParam(IsNot);
RUNTIME_BLOCK_activate(IsNot);
RUNTIME_BLOCK_END(IsNot);

void registerOpsBinaryBlocks() {
  REGISTER_CORE_BLOCK(Is);
  REGISTER_CORE_BLOCK(IsNot);
}
}; // namespace chainblocks

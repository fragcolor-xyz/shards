#ifndef CHAINBLOCKS_RUNTIME
#define CHAINBLOCKS_RUNTIME 1
#endif

#include "../runtime.hpp"

namespace chainblocks {
// Register Const
RUNTIME_CORE_BLOCK_FACTORY(Const);
RUNTIME_BLOCK_destroy(Const);
RUNTIME_BLOCK_inputTypes(Const);
RUNTIME_BLOCK_outputTypes(Const);
RUNTIME_BLOCK_parameters(Const);
RUNTIME_BLOCK_inferTypes(Const);
RUNTIME_BLOCK_setParam(Const);
RUNTIME_BLOCK_getParam(Const);
RUNTIME_BLOCK_activate(Const);
RUNTIME_BLOCK_END(Const);

// Register Sleep
RUNTIME_CORE_BLOCK_FACTORY(Sleep);
RUNTIME_BLOCK_inputTypes(Sleep);
RUNTIME_BLOCK_outputTypes(Sleep);
RUNTIME_BLOCK_parameters(Sleep);
RUNTIME_BLOCK_setParam(Sleep);
RUNTIME_BLOCK_getParam(Sleep);
RUNTIME_BLOCK_activate(Sleep);
RUNTIME_BLOCK_END(Sleep);

// Register Stop
RUNTIME_CORE_BLOCK_FACTORY(Stop);
RUNTIME_BLOCK_inputTypes(Stop);
RUNTIME_BLOCK_outputTypes(Stop);
RUNTIME_BLOCK_activate(Stop);
RUNTIME_BLOCK_END(Stop);

// Register Restart
RUNTIME_CORE_BLOCK_FACTORY(Restart);
RUNTIME_BLOCK_inputTypes(Restart);
RUNTIME_BLOCK_outputTypes(Restart);
RUNTIME_BLOCK_activate(Restart);
RUNTIME_BLOCK_END(Restart);

// Register Set
RUNTIME_CORE_BLOCK_FACTORY(Set);
RUNTIME_BLOCK_cleanup(Set);
RUNTIME_BLOCK_inputTypes(Set);
RUNTIME_BLOCK_outputTypes(Set);
RUNTIME_BLOCK_parameters(Set);
RUNTIME_BLOCK_inferTypes(Set);
RUNTIME_BLOCK_exposedVariables(Set);
RUNTIME_BLOCK_setParam(Set);
RUNTIME_BLOCK_getParam(Set);
RUNTIME_BLOCK_activate(Set);
RUNTIME_BLOCK_END(Set);

// Register Get
RUNTIME_CORE_BLOCK_FACTORY(Get);
RUNTIME_BLOCK_cleanup(Get);
RUNTIME_BLOCK_inputTypes(Get);
RUNTIME_BLOCK_outputTypes(Get);
RUNTIME_BLOCK_parameters(Get);
RUNTIME_BLOCK_inferTypes(Get);
RUNTIME_BLOCK_consumedVariables(Get);
RUNTIME_BLOCK_setParam(Get);
RUNTIME_BLOCK_getParam(Get);
RUNTIME_BLOCK_activate(Get);
RUNTIME_BLOCK_END(Get);

void registerBlocksCoreBlocks() {
  REGISTER_CORE_BLOCK(Const);
  REGISTER_CORE_BLOCK(Set);
  REGISTER_CORE_BLOCK(Get);
}
}; // namespace chainblocks
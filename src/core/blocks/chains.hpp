#pragma once

#include "shared.hpp"

namespace chainblocks
{
  static TypesInfo chainTypes = TypesInfo::FromMany(CBType::Chain, CBType::None);
  static ParamsInfo runChainParamsInfo = ParamsInfo(
    ParamsInfo::Param("Chain", "The chain to run.", CBTypesInfo(chainTypes)),
    ParamsInfo::Param("Once", "Runs this sub-chain only once within the parent chain execution cycle.", CBTypesInfo(boolInfo)),
    ParamsInfo::Param("Passthrough", "The input of this block will be the output. Always on if Detached.", CBTypesInfo(boolInfo)),
    ParamsInfo::Param("Detached", "Runs the sub-chain as a completely separate parallel chain in the same node.", CBTypesInfo(boolInfo))
  );

  struct RunChain
  {
    CBTypesInfo inputTypes()
    {
    return CBTypesInfo(anyInfo);
    }
    
    CBTypesInfo outputTypes()
    {
    return CBTypesInfo(anyInfo);
    }

    CBParametersInfo parameters()
    {
      return CBParametersInfo(runChainParamsInfo);
    }
  };
};

// Register GetItems
RUNTIME_CORE_BLOCK(chainblocks::RunChain, RunChain)
// RUNTIME_BLOCK_destroy(GetItems)
// RUNTIME_BLOCK_inputTypes(GetItems)
// RUNTIME_BLOCK_outputTypes(GetItems)
// RUNTIME_BLOCK_parameters(GetItems)
// RUNTIME_BLOCK_inferTypes(GetItems)
// RUNTIME_BLOCK_setParam(GetItems)
// RUNTIME_BLOCK_getParam(GetItems)
// RUNTIME_BLOCK_activate(GetItems)
RUNTIME_CORE_BLOCK_END(RunChain)

#pragma once

#include "shared.hpp"

namespace chainblocks {
static TypesInfo chainTypes = TypesInfo::FromMany(CBType::Chain, CBType::None);
static ParamsInfo runChainParamsInfo = ParamsInfo(
    ParamsInfo::Param("Chain", "The chain to run.", CBTypesInfo(chainTypes)),
    ParamsInfo::Param("Once",
                      "Runs this sub-chain only once within the parent chain "
                      "execution cycle.",
                      CBTypesInfo(boolInfo)),
    ParamsInfo::Param(
        "Passthrough",
        "The input of this block will be the output. Always on if Detached.",
        CBTypesInfo(boolInfo)),
    ParamsInfo::Param("Detached",
                      "Runs the sub-chain as a completely separate parallel "
                      "chain in the same node.",
                      CBTypesInfo(boolInfo)));

struct RunChain {
  CBChain *chain;
  bool once;
  bool doneOnce;
  bool passthrough;
  bool detached;

  CBTypesInfo inputTypes() { return CBTypesInfo(anyInfo); }

  CBTypesInfo outputTypes() { return CBTypesInfo(anyInfo); }

  CBParametersInfo parameters() { return CBParametersInfo(runChainParamsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      chain = value.payload.chainValue;
      break;
    case 1:
      once = value.payload.boolValue;
      break;
    case 2:
      passthrough = value.payload.boolValue;
      break;
    case 3:
      detached = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(chain);
      break;
    case 1:
      return Var(once);
      break;
    case 2:
      return Var(passthrough);
      break;
    case 3:
      return Var(detached);
      break;
    default:
      break;
    }
    return Var();
  }

  CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    if (passthrough || !chain)
      return inputType;

    // We need to validate the sub chain to figure it out!

    CBTypeInfo outputType = validateConnections(
        chain,
        [](const CBlock *errorBlock, const char *errorTxt, bool nonfatalWarning,
           void *userData) {
          if (!nonfatalWarning) {
            LOG(ERROR) << "RunChain: failed inner chain validation, error: "
                       << errorTxt;
            throw CBException("RunChain: failed inner chain validation");
          } else {
            LOG(INFO) << "RunChain: warning during inner chain validation: "
                      << errorTxt;
          }
        },
        this, inputType);

    return outputType;
  }

  CBVar activate(CBContext *context, CBVar input) {
    if (!chain)
      return input;

    if (!doneOnce) {
      if (once)
        doneOnce = true;

      if (detached) {
        if (!chainblocks::isRunning(chain)) {
          chainblocks::CurrentChain->node->schedule(chain, input);
        }
        return input;
      } else {
        // Run within the root flow, just call runChain

        chain->finished = false; // Reset finished flag (atomic)
        auto runRes = runChain(chain, context, input);
        chain->finishedOutput =
            runRes.output;      // Write result before setting flag
        chain->finished = true; // Set finished flag (atomic)
        if (runRes.state == Failed || context->aborted) {
          return Var::Stop();
        } else if (passthrough) {
          return input;
        } else {
          return runRes.output;
        }
      }
    } else {
      return input;
    }
  }

  void cleanup() {
    if (chain)
      chainblocks::stop(chain);
    doneOnce = false;
  }
};
}; // namespace chainblocks

// Register GetItems
RUNTIME_CORE_BLOCK(chainblocks::RunChain, RunChain)
RUNTIME_BLOCK_inputTypes(RunChain) RUNTIME_BLOCK_outputTypes(RunChain)
    RUNTIME_BLOCK_parameters(RunChain) RUNTIME_BLOCK_inferTypes(RunChain)
        RUNTIME_BLOCK_setParam(RunChain) RUNTIME_BLOCK_getParam(RunChain)
            RUNTIME_BLOCK_activate(RunChain) RUNTIME_BLOCK_cleanup(RunChain)
                RUNTIME_CORE_BLOCK_END(RunChain)

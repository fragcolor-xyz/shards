/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "rigtorp/SPSCQueue.h"
#include "shared.hpp"
#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace fs = std::filesystem;

namespace chainblocks {
enum RunChainMode { Inline, Detached, Stepped };

struct ChainBase {
  static inline TypesInfo chainTypes =
      TypesInfo::FromMany(false, CBType::Chain, CBType::String, CBType::None);

  static inline ParamsInfo waitChainParamsInfo = ParamsInfo(
      ParamsInfo::Param("Chain", "The chain to run.", CBTypesInfo(chainTypes)),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CBTypesInfo(SharedTypes::boolInfo)),
      ParamsInfo::Param(
          "Passthrough",
          "The input of this block will be the output. Always on if Detached.",
          CBTypesInfo(SharedTypes::boolInfo)));

  static inline ParamsInfo runChainParamsInfo = ParamsInfo(
      ParamsInfo::Param("Chain", "The chain to run.", CBTypesInfo(chainTypes)),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CBTypesInfo(SharedTypes::boolInfo)),
      ParamsInfo::Param(
          "Passthrough",
          "The input of this block will be the output. Not used if Detached.",
          CBTypesInfo(SharedTypes::boolInfo)),
      ParamsInfo::Param(
          "Mode",
          "The way to run the chain. Inline: will run the sub chain inline "
          "within the root chain, a pause in the child chain will pause the "
          "root "
          "too; Detached: will run the chain separately in the same node, a "
          "pause in this chain will not pause the root; Stepped: the chain "
          "will "
          "run as a child, the root will tick the chain every activation of "
          "this "
          "block and so a child pause won't pause the root.",
          CBTypesInfo(SharedTypes::runChainModeInfo)));

  static inline ParamsInfo chainloaderParamsInfo = ParamsInfo(
      ParamsInfo::Param(
          "File", "The chainblocks lisp file of the chain to run and watch.",
          CBTypesInfo(SharedTypes::strInfo)),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CBTypesInfo(SharedTypes::boolInfo)),
      ParamsInfo::Param(
          "Mode",
          "The way to run the chain. Inline: will run the sub chain inline "
          "within the root chain, a pause in the child chain will pause the "
          "root "
          "too; Detached: will run the chain separately in the same node, a "
          "pause in this chain will not pause the root; Stepped: the chain "
          "will "
          "run as a child, the root will tick the chain every activation of "
          "this "
          "block and so a child pause won't pause the root.",
          CBTypesInfo(SharedTypes::runChainModeInfo)));

  static inline ParamsInfo chainOnlyParamsInfo = ParamsInfo(
      ParamsInfo::Param("Chain", "The chain to run.", CBTypesInfo(chainTypes)));

  CBChain *chain;
  CBVar chainref;
  bool once;
  bool doneOnce;
  bool passthrough;
  RunChainMode mode;
  CBValidationResult chainValidation{};

  void destroy() { stbds_arrfree(chainValidation.exposedInfo); }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  static inline thread_local std::set<CBChain *> visiting;

  void tryStopChain() {
    if (chain) {
      // avoid stackoverflow
      if (visiting.count(chain))
        return;

      visiting.insert(chain);
      chainblocks::stop(chain);
      visiting.erase(chain);
    }
  }

  CBTypeInfo compose(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    CBInstanceData data{};
    data.inputType = inputType;
    data.consumables = consumables;

    // Free any previous result!
    stbds_arrfree(chainValidation.exposedInfo);
    chainValidation.exposedInfo = nullptr;

    // Actualize the chain here...
    if (chainref.valueType == CBType::Chain) {
      chain = chainref.payload.chainValue;
    } else if (chainref.valueType == String) {
      chain = chainblocks::Globals::GlobalChains[chainref.payload.stringValue];
    } else {
      chain = nullptr;
    }

    // Easy case, no chain...
    if (!chain)
      return inputType;

    // avoid stackoverflow
    if (visiting.count(chain))
      return inputType; // we don't know yet...

    visiting.insert(chain);

    // We need to validate the sub chain to figure it out!
    chainValidation = validateConnections(
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
        this, data);

    visiting.erase(chain);

    return passthrough || mode == RunChainMode::Detached
               ? inputType
               : chainValidation.outputType;
  }
};

struct WaitChain : public ChainBase {
  void cleanup() { doneOnce = false; }

  static CBParametersInfo parameters() {
    return CBParametersInfo(waitChainParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      cloneVar(chainref, value);
      break;
    case 1:
      once = value.payload.boolValue;
      break;
    case 2:
      passthrough = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return chainref;
    case 1:
      return Var(once);
    case 2:
      return Var(passthrough);
    default:
      break;
    }
    return Var();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain)) {
      return input;
    } else if (!doneOnce) {
      if (once)
        doneOnce = true;

      while (!hasEnded(chain)) {
        cbpause(0.0);
      }

      return passthrough ? input : chain->finishedOutput;
    } else {
      return input;
    }
  }
};

struct ContinueChain : public ChainBase {
  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Chain", "The name of the chain to switch to.",
                        CBTypesInfo(ChainBase::chainTypes)));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypeInfo compose(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    ChainBase::compose(inputType, consumables);
    return inputType;
  }

  void setParam(int index, CBVar value) { cloneVar(chainref, value); }

  CBVar getParam(int index) { return chainref; }

  // NO cleanup, other chains reference this chain but should not stop it
  // An arbitrary chain should be able to resume it!
  // Cleanup mechanics still to figure, for now ref count of the actual chain
  // symbol

  CBVar activate(CBContext *context, const CBVar &input) {
    // assign current flow to the chain we are going to
    chain->flow = context->chain->flow;
    // if we have a node also make sure chain knows about it
    chain->node = context->chain->node;
    // assign the new chain as current chain on the flow
    chain->flow->chain = chain;

    // Allow to re run chains
    if (chainblocks::hasEnded(chain)) {
      chainblocks::stop(chain);
    }

    // Prepare if no callc was called
    if (!chain->coro) {
      chainblocks::prepare(chain);
    }

    // Start it if not started, this will tick it once!
    if (!chainblocks::isRunning(chain)) {
      chainblocks::start(chain, input);
    }

    // And normally we just delegate the CBNode + CBFlow
    chainblocks::suspend(context, 0);

    return input;
  }
};

struct ChainRunner : public ChainBase {
  CBFlow _steppedFlow;

  // Only chain runners should expose varaibles to the context
  CBExposedTypesInfo exposedVariables() {
    // Only inline mode ensures that variables will be really avail
    // step and detach will run at different timing
    return mode == RunChainMode::Inline ? chainValidation.exposedInfo : nullptr;
  }

  void cleanup() {
    tryStopChain();
    _steppedFlow.chain = nullptr;
    doneOnce = false;
  }

  ALWAYS_INLINE void activateDetached(CBContext *context, const CBVar &input) {
    if (!chainblocks::isRunning(chain)) {
      // validated during infer not here! (false)
      context->chain->node->schedule(chain, input, false);
    }
  }

  ALWAYS_INLINE void activateStepMode(CBContext *context, const CBVar &input) {
    // We want to allow a sub flow within the stepped chain
    if (!_steppedFlow.chain) {
      _steppedFlow.chain = chain;
      chain->flow = &_steppedFlow;
      chain->node = context->chain->node;
    }

    // Allow to re run chains
    if (chainblocks::hasEnded(chain)) {
      // stop the root
      chainblocks::stop(chain);

      // swap flow to the root chain
      _steppedFlow.chain = chain;
      chain->flow = &_steppedFlow;
      chain->node = context->chain->node;
    }

    // Prepare if no callc was called
    if (!chain->coro) {
      chainblocks::prepare(chain);
    }

    // Ticking or starting
    if (!chainblocks::isRunning(chain)) {
      chainblocks::start(chain, input);
    } else {
      // tick the flow one rather then directly chain!
      chainblocks::tick(_steppedFlow.chain, input);
    }
  }
};

struct RunChain : public ChainRunner {
  static CBParametersInfo parameters() {
    return CBParametersInfo(runChainParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      cloneVar(chainref, value);
      break;
    case 1:
      once = value.payload.boolValue;
      break;
    case 2:
      passthrough = value.payload.boolValue;
      break;
    case 3:
      mode = RunChainMode(value.payload.enumValue);
      break;
    default:
      break;
    }
    // make sure to reset!
    cleanup();
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return chainref;
    case 1:
      return Var(once);
    case 2:
      return Var(passthrough);
    case 3:
      return Var::Enum(mode, 'frag', 'runC');
    default:
      break;
    }
    return Var();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    if (!doneOnce) {
      if (once)
        doneOnce = true;

      if (mode == RunChainMode::Detached) {
        activateDetached(context, input);
        return input;
      } else if (mode == RunChainMode::Stepped) {
        activateStepMode(context, input);
        return passthrough ? input : chain->previousOutput;
      } else {
        // Run within the root flow
        chain->flow = context->chain->flow;
        chain->node = context->chain->node;
        auto runRes = runSubChain(chain, context, input);
        if (unlikely(runRes.state == Failed || context->aborted)) {
          return StopChain;
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
};

struct ChainLoader : public ChainRunner {
  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Provider", "The chainblocks chain provider.",
                        ChainProvider::Info),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CBTypesInfo(SharedTypes::boolInfo)),
      ParamsInfo::Param(
          "Mode",
          "The way to run the chain. Inline: will run the sub chain inline "
          "within the root chain, a pause in the child chain will pause the "
          "root "
          "too; Detached: will run the chain separately in the same node, a "
          "pause in this chain will not pause the root; Stepped: the chain "
          "will "
          "run as a child, the root will tick the chain every activation of "
          "this "
          "block and so a child pause won't pause the root.",
          CBTypesInfo(SharedTypes::runChainModeInfo)));

  CBChainProvider *_provider;

  CBTypeInfo _inputTypeCopy{};
  IterableExposedInfo _consumablesCopy;

  CBTypeInfo compose(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    _inputTypeCopy = inputType;
    const IterableExposedInfo consumablesStb(consumables);
    // copy consumables
    _consumablesCopy = consumablesStb;
    return inputType;
  }

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      cleanup(); // stop current
      if (value.valueType == Object) {
        _provider = (CBChainProvider *)value.payload.objectValue;
      } else {
        _provider = nullptr;
      }
      break;
    case 1:
      once = value.payload.boolValue;
      break;
    case 2:
      mode = RunChainMode(value.payload.enumValue);
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      if (_provider) {
        return Var::Object(_provider, 'frag', 'chnp');
      } else {
        return Var();
      }
      break;
    case 1:
      return Var(once);
      break;
    case 2:
      return Var::Enum(mode, 'frag', 'runC');
      break;
    default:
      break;
    }
    return Var();
  }

  void cleanup() {
    ChainRunner::cleanup();

    if (_provider && chain) {
      _provider->release(_provider, chain);
      chain = nullptr;
    }

    if (_provider)
      _provider->reset(_provider);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!_provider))
      return input;

    if (unlikely(!_provider->ready(_provider))) {
      CBInstanceData data{};
      data.inputType = _inputTypeCopy;
      data.consumables = _consumablesCopy();
      _provider->setup(_provider, context->chain->node->currentPath.c_str(),
                       data);
    }

    if (unlikely(_provider->updated(_provider))) {
      auto update = _provider->acquire(_provider);
      if (unlikely(update.error != nullptr)) {
        LOG(ERROR) << "Failed to reload a chain via ChainLoader, reason: "
                   << update.error;
      } else {
        if (chain) {
          // stop and release previous version
          chainblocks::stop(chain);
          _provider->release(_provider, chain);
        }
        chain = update.chain;
      }
    }

    if (unlikely(!chain))
      return input;

    if (!doneOnce) {
      if (once)
        doneOnce = true;

      if (mode == RunChainMode::Detached) {
        activateDetached(context, input);
        return input;
      } else if (mode == RunChainMode::Stepped) {
        activateStepMode(context, input);
        return input;
      } else {
        // Run within the root flow
        chain->flow = context->chain->flow;
        chain->node = context->chain->node;
        auto runRes = runSubChain(chain, context, input);
        if (unlikely(runRes.state == Failed || context->aborted)) {
          return StopChain;
        } else {
          return input;
        }
      }
    } else {
      return input;
    }
  }
};

typedef BlockWrapper<ContinueChain> ContinueChainBlock;
typedef BlockWrapper<WaitChain> WaitChainBlock;
typedef BlockWrapper<RunChain> RunChainBlock;
typedef BlockWrapper<ChainLoader> ChainLoaderBlock;

void registerChainsBlocks() {
  registerBlock("ContinueChain", &ContinueChainBlock::create);
  registerBlock("WaitChain", &WaitChainBlock::create);
  registerBlock("RunChain", &RunChainBlock::create);
  registerBlock("ChainLoader", &ChainLoaderBlock::create);
}
}; // namespace chainblocks

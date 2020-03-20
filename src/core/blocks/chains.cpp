/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "rigtorp/SPSCQueue.h"
#include "shared.hpp"
#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace fs = std::filesystem;

namespace chainblocks {
enum RunChainMode { Inline, Detached, Stepped };

struct ChainBase {
  typedef EnumInfo<RunChainMode> RunChainModeInfo;
  static inline RunChainModeInfo runChainModeInfo{"RunChainMode", 'frag',
                                                  'runC'};
  static inline Type ModeType{
      {CBType::Enum, {.enumeration = {.vendorId = 'frag', .typeId = 'runC'}}}};

  static inline Types ChainTypes{
      {CoreInfo::ChainType, CoreInfo::StringType, CoreInfo::NoneType}};

  static inline ParamsInfo waitChainParamsInfo = ParamsInfo(
      ParamsInfo::Param("Chain", "The chain to run.", ChainTypes),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CoreInfo::BoolType),
      ParamsInfo::Param(
          "Passthrough",
          "The input of this block will be the output. Always on if Detached.",
          CoreInfo::BoolType));

  static inline ParamsInfo runChainParamsInfo = ParamsInfo(
      ParamsInfo::Param("Chain", "The chain to run.", ChainTypes),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CoreInfo::BoolType),
      ParamsInfo::Param(
          "Passthrough",
          "The input of this block will be the output. Not used if Detached.",
          CoreInfo::BoolType),
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
          ModeType));

  static inline ParamsInfo chainloaderParamsInfo = ParamsInfo(
      ParamsInfo::Param(
          "File", "The chainblocks lisp file of the chain to run and watch.",
          CoreInfo::StringType),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CoreInfo::BoolType),
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
          ModeType));

  static inline ParamsInfo chainOnlyParamsInfo =
      ParamsInfo(ParamsInfo::Param("Chain", "The chain to run.", ChainTypes));

  CBVar chainref;
  std::shared_ptr<CBChain> chain;
  bool once;
  bool doneOnce;
  bool passthrough;
  RunChainMode mode;
  CBValidationResult chainValidation{};

  void destroy() { chainblocks::arrayFree(chainValidation.exposedInfo); }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline thread_local std::set<CBChain *> visiting;

  void tryStopChain() {
    if (chain) {
      // avoid stackoverflow
      if (visiting.count(chain.get()))
        return;

      visiting.insert(chain.get());
      chainblocks::stop(chain.get());
      visiting.erase(chain.get());
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    // Free any previous result!
    chainblocks::arrayFree(chainValidation.exposedInfo);

    // Actualize the chain here...
    if (chainref.valueType == CBType::Chain) {
      chain = CBChain::sharedFromRef(chainref.payload.chainValue);
    } else if (chainref.valueType == String) {
      chain = chainblocks::Globals::GlobalChains[chainref.payload.stringValue];
    } else {
      chain = nullptr;
    }

    // Easy case, no chain...
    if (!chain)
      return data.inputType;

    // avoid stackoverflow
    if (visiting.count(chain.get()))
      return data.inputType; // we don't know yet...

    visiting.insert(chain.get());

    // We need to validate the sub chain to figure it out!
    chainValidation = validateConnections(
        chain.get(),
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

    visiting.erase(chain.get());

    return passthrough || mode == RunChainMode::Detached
               ? data.inputType
               : chainValidation.outputType;
  }
};

struct WaitChain : public ChainBase {
  CBVar _output{};

  void cleanup() {
    doneOnce = false;
    destroyVar(_output);
  }

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

      while (isRunning(chain.get())) {
        suspend(context, 0);
      }

      if (passthrough) {
        return input;
      } else {
        cloneVar(_output, chain->finishedOutput);
        return _output;
      }
    } else {
      return input;
    }
  }
};

struct Resume : public ChainBase {
  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Chain", "The name of the chain to switch to.", ChainTypes));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    ChainBase::compose(data);
    return data.inputType;
  }

  void setParam(int index, CBVar value) { cloneVar(chainref, value); }

  CBVar getParam(int index) { return chainref; }

  // NO cleanup, other chains reference this chain but should not stop it
  // An arbitrary chain should be able to resume it!
  // Cleanup mechanics still to figure, for now ref count of the actual chain
  // symbol, TODO maybe use CBVar refcount!

  CBVar activate(CBContext *context, const CBVar &input) {
    // assign current flow to the chain we are going to
    chain->flow = context->main->flow;
    // if we have a node also make sure chain knows about it
    chain->node = context->main->node;
    // assign the new chain as current chain on the flow
    chain->flow->chain = chain.get();

    // Allow to re run chains
    if (chainblocks::hasEnded(chain.get())) {
      chainblocks::stop(chain.get());
    }

    // Prepare if no callc was called
    if (!chain->coro) {
      chainblocks::prepare(chain.get());
    }

    // Start it if not started
    if (!chainblocks::isRunning(chain.get())) {
      chainblocks::start(chain.get(), input);
    }

    // And normally we just delegate the CBNode + CBFlow
    // the following will suspend this current chain
    // and in node tick when re-evaluated tick will
    // resume with the chain we just set above!
    chainblocks::suspend(context, 0);

    return input;
  }
};

struct Start : public Resume {
  CBVar activate(CBContext *context, const CBVar &input) {
    // assign current flow to the chain we are going to
    chain->flow = context->main->flow;
    // if we have a node also make sure chain knows about it
    chain->node = context->main->node;
    // assign the new chain as current chain on the flow
    chain->flow->chain = chain.get();

    // ensure chain is not running, we start from top
    chainblocks::stop(chain.get());

    // Prepare
    chainblocks::prepare(chain.get());

    // Start
    chainblocks::start(chain.get(), input);

    // And normally we just delegate the CBNode + CBFlow
    // the following will suspend this current chain
    // and in node tick when re-evaluated tick will
    // resume with the chain we just set above!
    chainblocks::suspend(context, 0);

    return input;
  }
};

struct BaseRunner : public ChainBase {
  CBFlow _steppedFlow;

  // Only chain runners should expose varaibles to the context
  CBExposedTypesInfo exposedVariables() {
    // Only inline mode ensures that variables will be really avail
    // step and detach will run at different timing
    CBExposedTypesInfo empty{};
    return mode == RunChainMode::Inline ? chainValidation.exposedInfo : empty;
  }

  void cleanup() {
    tryStopChain();
    _steppedFlow.chain = nullptr;
    doneOnce = false;
  }

  void doWarmup(CBContext *context) {
    auto current = context->chainStack.back();
    if (mode == RunChainMode::Inline && chain && current != chain.get()) {
      context->chainStack.push_back(chain.get());
      chain->warmup(context);
      context->chainStack.pop_back();
    }
  }

  ALWAYS_INLINE void activateDetached(CBContext *context, const CBVar &input) {
    if (!chainblocks::isRunning(chain.get())) {
      // validated during infer not here! (false)
      context->main->node->schedule(chain.get(), input, false);
    }
  }

  ALWAYS_INLINE void activateStepMode(CBContext *context, const CBVar &input) {
    // We want to allow a sub flow within the stepped chain
    if (!_steppedFlow.chain) {
      _steppedFlow.chain = chain.get();
      chain->flow = &_steppedFlow;
      chain->node = context->main->node;
    }

    // Allow to re run chains
    if (chainblocks::hasEnded(chain.get())) {
      // stop the root
      chainblocks::stop(chain.get());

      // swap flow to the root chain
      _steppedFlow.chain = chain.get();
      chain->flow = &_steppedFlow;
      chain->node = context->main->node;
    }

    // Prepare if no callc was called
    if (!chain->coro) {
      chainblocks::prepare(chain.get());
    }

    // Starting
    if (!chainblocks::isRunning(chain.get())) {
      chainblocks::start(chain.get(), input);
    }

    // tick the flow one rather then directly the chain!
    chainblocks::tick(_steppedFlow.chain, input);
  }
};

struct RunChain : public BaseRunner {
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

  void warmup(CBContext *context) { doWarmup(context); }

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
        chain->flow = context->main->flow;
        chain->node = context->main->node;
        auto runRes = runSubChain(chain.get(), context, input);
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

template <class T> struct BaseLoader : public BaseRunner {
  CBTypeInfo _inputTypeCopy{};
  IterableExposedInfo _sharedCopy;

  CBTypeInfo compose(const CBInstanceData &data) {
    _inputTypeCopy = data.inputType;
    const IterableExposedInfo sharedStb(data.shared);
    // copy shared
    _sharedCopy = sharedStb;
    return data.inputType;
  }

  void setParam(int index, CBVar value) {
    switch (index) {
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
    case 1:
      return Var(once);
    case 2:
      return Var::Enum(mode, 'frag', 'runC');
    default:
      break;
    }
    return Var();
  }

  void cleanup() { BaseRunner::cleanup(); }

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
        return input;
      } else {
        // Run within the root flow
        chain->flow = context->main->flow;
        chain->node = context->main->node;
        auto runRes = runSubChain(chain.get(), context, input);
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

struct ChainLoader : public BaseLoader<ChainLoader> {
  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Provider", "The chainblocks chain provider.",
                        ChainProvider::ProviderOrNone),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CoreInfo::BoolType),
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
          ModeType));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  CBChainProvider *_provider;

  void setParam(int index, CBVar value) {
    if (index == 0) {
      cleanup(); // stop current
      if (value.valueType == Object) {
        _provider = (CBChainProvider *)value.payload.objectValue;
      } else {
        _provider = nullptr;
      }
    } else {
      BaseLoader<ChainLoader>::setParam(index, value);
    }
  }

  CBVar getParam(int index) {
    if (index == 0) {
      if (_provider) {
        return Var::Object(_provider, 'frag', 'chnp');
      } else {
        return Var();
      }
    } else {
      return BaseLoader<ChainLoader>::getParam(index);
    }
  }

  void cleanup() { BaseLoader<ChainLoader>::cleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!_provider))
      return input;

    if (unlikely(!_provider->ready(_provider))) {
      CBInstanceData data{};
      data.inputType = _inputTypeCopy;
      data.shared = _sharedCopy;
      _provider->setup(_provider, Globals::RootPath.c_str(), data);
    }

    if (unlikely(_provider->updated(_provider))) {
      auto update = _provider->acquire(_provider);
      if (unlikely(update.error != nullptr)) {
        LOG(ERROR) << "Failed to reload a chain via ChainLoader, reason: "
                   << update.error;
      } else {
        if (chain) {
          // stop and release previous version
          chainblocks::stop(chain.get());
          _provider->release(_provider, chain.get());
        }
        chain.reset(update.chain);

        doWarmup(context);
      }
    }

    return BaseLoader<ChainLoader>::activate(context, input);
  }
};

struct ChainRunner : public BaseLoader<ChainLoader> {
  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Chain", "The chain variable to compose and run.",
                        CoreInfo::ChainVarType),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CoreInfo::BoolType),
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
          ModeType));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  ParamVar _chain{};
  std::size_t _chainHash = 0;

  void setParam(int index, CBVar value) {
    if (index == 0) {
      _chain = value;
    } else {
      BaseLoader<ChainLoader>::setParam(index, value);
    }
  }

  CBVar getParam(int index) {
    if (index == 0) {
      return _chain;
    } else {
      return BaseLoader<ChainLoader>::getParam(index);
    }
  }

  void cleanup() {
    BaseLoader<ChainLoader>::cleanup();
    _chain.cleanup();
  }

  void warmup(CBContext *context) { _chain.warmup(context); }

  void composeChain() {
    CBInstanceData data{};
    data.inputType = _inputTypeCopy;
    data.shared = _sharedCopy;

    // avoid stackoverflow
    if (visiting.count(chain.get()))
      return; // we don't know yet...

    visiting.insert(chain.get());

    // We need to validate the sub chain to figure it out!
    auto res = validateConnections(
        chain.get(),
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

    visiting.erase(chain.get());
    chainblocks::arrayFree(res.exposedInfo);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto chainVar = _chain.get();
    chain = CBChain::sharedFromRef(chainVar.payload.chainValue);
    if (unlikely(!chain))
      return input;

    if (_chainHash == 0 || _chainHash != chain->composedHash) {
      // Compose and hash in a thread
      auto asyncRes = std::async(std::launch::async, [this, chainVar]() {
        composeChain();
        chain->composedHash = std::hash<CBVar>()(chainVar);
      });

      // Wait suspending!
      while (true) {
        auto state = asyncRes.wait_for(std::chrono::seconds(0));
        if (state == std::future_status::ready)
          break;

        chainblocks::suspend(context, 0);
      }

      // This should throw if we had excetptions
      asyncRes.get();

      _chainHash = chain->composedHash;

      doWarmup(context);
    }

    return BaseLoader<ChainLoader>::activate(context, input);
  }
};

void registerChainsBlocks() {
  REGISTER_CBLOCK("Resume", Resume);
  REGISTER_CBLOCK("Start", Start);
  REGISTER_CBLOCK("WaitChain", WaitChain);
  REGISTER_CBLOCK("RunChain", RunChain);
  REGISTER_CBLOCK("ChainLoader", ChainLoader);
  REGISTER_CBLOCK("ChainRunner", ChainRunner);
}
}; // namespace chainblocks

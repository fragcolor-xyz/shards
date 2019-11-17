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

  CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    // Free any previous result!
    stbds_arrfree(chainValidation.exposedInfo);
    chainValidation.exposedInfo = nullptr;

    // Actualize the chain here...
    if (chainref.valueType == Chain) {
      chain = chainref.payload.chainValue;
    } else if (chainref.valueType == String) {
      chain = chainblocks::GlobalChains[chainref.payload.stringValue];
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
        this, inputType,
        mode == RunChainMode::Inline
            ? consumables
            : nullptr); // detached don't share context!

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

  CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    ChainBase::inferTypes(inputType, consumables);
    return inputType;
  }

  void setParam(int index, CBVar value) { cloneVar(chainref, value); }

  CBVar getParam(int index) { return chainref; }

  CBVar activate(CBContext *context, const CBVar &input) {
    // assign current flow to the chain we are going to
    chain->flow = context->chain->flow;
    // assign the new chain as current chain on the flow
    chain->flow->chain = chain;

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
    return mode == RunChainMode::Inline ? chainValidation.exposedInfo : nullptr;
  }

  void cleanup() {
    if (chain)
      chainblocks::stop(chain);
    doneOnce = false;
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

  void cleanup() {
    _steppedFlow.chain = nullptr;
    ChainRunner::cleanup();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    if (!doneOnce) {
      if (once)
        doneOnce = true;

      if (mode == RunChainMode::Detached) {
        if (!chainblocks::isRunning(chain)) {
          // validated during infer
          context->chain->node->schedule(chain, input, false);
        }
        return input;
      } else if (mode == RunChainMode::Stepped) {
        // We want to allow a sub flow within the stepped chain
        if (!_steppedFlow.chain) {
          _steppedFlow.chain = chain;
          chain->flow = &_steppedFlow;
          chain->node = context->chain->node;
        }

        // Allow to re run chains
        if (chainblocks::hasEnded(chain)) {
          chainblocks::stop(chain);
          // chain is still the root!
          _steppedFlow.chain = chain;
          chain->flow = &_steppedFlow;
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

struct ChainLoadResult {
  bool hasError;
  std::string errorMsg;
  CBChain *chain;
  void *env;
};

struct ChainFileWatcher {
  std::atomic_bool running;
  std::thread worker;
  std::string fileName;
  std::string path;
  rigtorp::SPSCQueue<ChainLoadResult> results;
  boost::lockfree::queue<void *> envs_gc;

  explicit ChainFileWatcher(std::string &file, std::string currentPath)
      : running(true), fileName(file), path(currentPath), results(2),
        envs_gc(2) {
    worker = std::thread([this] {
      decltype(fs::last_write_time(fs::path())) lastWrite{};
      auto localRoot = std::filesystem::path(path);
      auto localRootStr = localRoot.string();

      if (!Lisp::Create) {
        LOG(ERROR) << "Failed to load lisp interpreter";
        return;
      }

      while (running) {
        try {
          fs::path p(fileName);
          if (path.size() > 0 && p.is_relative()) {
            // complete path with current path if any
            p = localRoot / p;
          }

          if (!fs::exists(p)) {
            LOG(INFO) << "A ChainLoader loaded script path does not exist: "
                      << p;
          } else if (fs::is_regular_file(p) &&
                     fs::last_write_time(p) != lastWrite) {
            // make sure to store last write time
            // before any possible error!
            auto writeTime = fs::last_write_time(p);
            lastWrite = writeTime;

            std::ifstream lsp(p.c_str());
            std::string str((std::istreambuf_iterator<char>(lsp)),
                            std::istreambuf_iterator<char>());

            // envs are not being cleaned properly for now
            // symbols have high ref count, need to fix
            auto env = Lisp::Create(localRootStr.c_str());
            auto v = Lisp::Eval(env, str.c_str());
            if (v.valueType != Chain) {
              LOG(ERROR) << "Lisp::Eval did not return a CBChain";
              Lisp::Destroy(env);
              continue;
            }

            auto chain = v.payload.chainValue;

            // run validation to infertypes and specialize
            auto chainValidation = validateConnections(
                chain,
                [](const CBlock *errorBlock, const char *errorTxt,
                   bool nonfatalWarning, void *userData) {
                  if (!nonfatalWarning) {
                    auto msg =
                        "RunChain: failed inner chain validation, error: " +
                        std::string(errorTxt);
                    throw chainblocks::CBException(msg);
                  } else {
                    LOG(INFO)
                        << "RunChain: warning during inner chain validation: "
                        << errorTxt;
                  }
                },
                nullptr);
            stbds_arrfree(chainValidation.exposedInfo);

            ChainLoadResult result = {false, "", chain, env};
            results.push(result);
          }

          // Collect garbage
          if (!envs_gc.empty()) {
            void *genv;
            if (envs_gc.pop(genv)) {
              Lisp::Destroy(genv);
            }
          }

          // sleep some
          chainblocks::sleep(2.0);
        } catch (std::exception &e) {
          ChainLoadResult result = {true, e.what(), nullptr, nullptr};
          results.push(result);
        } catch (...) {
          ChainLoadResult result = {true, "general error", nullptr, nullptr};
          results.push(result);
        }
      }

      // Collect garbage
      while (!envs_gc.empty()) {
        void *genv;
        if (envs_gc.pop(genv)) {
          Lisp::Destroy(genv);
        }
      }
    });
  }

  ~ChainFileWatcher() {
    running = false;
    worker.join();
  }
};

struct ChainLoader : public ChainRunner {
  std::string fileName;
  std::unique_ptr<ChainFileWatcher> watcher;
  void *currentEnv;

  ChainLoader() : ChainRunner(), watcher(nullptr), currentEnv(nullptr) {}

  CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    return inputType;
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(chainloaderParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      fileName = value.payload.stringValue;
      cleanup(); // stop current watcher
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
      return Var(fileName);
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
    if (chain) {
      chainblocks::stop(chain);
      // don't delete chains.. let env do it
      watcher->envs_gc.push(currentEnv);
      chain = nullptr;
    }

    watcher.reset(nullptr);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!watcher) {
      watcher.reset(
          new ChainFileWatcher(fileName, context->chain->node->currentPath));
    }

    if (!watcher->results.empty()) {
      auto result = watcher->results.front();
      if (unlikely(result->hasError)) {
        LOG(ERROR) << "Failed to reload a chain via ChainLoader, reason: "
                   << result->errorMsg;
      } else {
        if (chain) {
          chainblocks::stop(chain);
          // don't delete chains.. let env do it
          while (!watcher->envs_gc.push(currentEnv)) {
            cbpause(0.0);
          }
        }
        chain = result->chain;
        currentEnv = result->env;
      }
      watcher->results.pop();
    }

    if (unlikely(!chain))
      return input;

    if (!doneOnce) {
      if (once)
        doneOnce = true;

      if (mode == RunChainMode::Detached) {
        if (!chainblocks::isRunning(chain)) {
          // validated during infer
          context->chain->node->schedule(chain, input, false);
        }
        return input;
      } else if (mode == RunChainMode::Stepped) {
        // We want to allow a sub flow within the stepped chain
        if (!_steppedFlow.chain) {
          _steppedFlow.chain = chain;
          chain->flow = &_steppedFlow;
          chain->node = context->chain->node;
        }

        // Allow to re run chains
        if (chainblocks::hasEnded(chain)) {
          chainblocks::stop(chain);
          // chain is still the root!
          _steppedFlow.chain = chain;
          chain->flow = &_steppedFlow;
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

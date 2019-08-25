#include "3rdparty/SPSCQueue.h"
#include "shared.hpp"
#include <boost/filesystem.hpp>
#include <chrono>
#include <fstream>
#include <memory>
#include <thread>

namespace bfs = boost::filesystem;

namespace chainblocks {
static TypesInfo chainTypes = TypesInfo::FromMany(CBType::Chain, CBType::None);

static ParamsInfo waitChainParamsInfo = ParamsInfo(
    ParamsInfo::Param("Chain", "The chain to run.", CBTypesInfo(chainTypes)),
    ParamsInfo::Param("Once",
                      "Runs this sub-chain only once within the parent chain "
                      "execution cycle.",
                      CBTypesInfo(SharedTypes::boolInfo)),
    ParamsInfo::Param(
        "Passthrough",
        "The input of this block will be the output. Always on if Detached.",
        CBTypesInfo(SharedTypes::boolInfo)));

static ParamsInfo runChainParamsInfo = ParamsInfo(
    ParamsInfo::Param("Chain", "The chain to run.", CBTypesInfo(chainTypes)),
    ParamsInfo::Param("Once",
                      "Runs this sub-chain only once within the parent chain "
                      "execution cycle.",
                      CBTypesInfo(SharedTypes::boolInfo)),
    ParamsInfo::Param(
        "Passthrough",
        "The input of this block will be the output. Always on if Detached.",
        CBTypesInfo(SharedTypes::boolInfo)),
    ParamsInfo::Param("Detached",
                      "Runs the sub-chain as a completely separate parallel "
                      "chain in the same node.",
                      CBTypesInfo(SharedTypes::boolInfo)));

static ParamsInfo chainloaderParamsInfo = ParamsInfo(
    ParamsInfo::Param("File", "The json file of the chain to run and watch.",
                      CBTypesInfo(SharedTypes::strInfo)),
    ParamsInfo::Param("Once",
                      "Runs this sub-chain only once within the parent chain "
                      "execution cycle.",
                      CBTypesInfo(SharedTypes::boolInfo)),
    ParamsInfo::Param("Detached",
                      "Runs the sub-chain as a completely separate parallel "
                      "chain in the same node.",
                      CBTypesInfo(SharedTypes::boolInfo)));

static ParamsInfo chainOnlyParamsInfo = ParamsInfo(
    ParamsInfo::Param("Chain", "The chain to run.", CBTypesInfo(chainTypes)));

struct ChainBase {
  CBChain *chain;
  bool once;
  bool doneOnce;
  bool passthrough;
  bool detached;
  CBValidationResult chainValidation{};

  void destroy() { stbds_arrfree(chainValidation.exposedInfo); }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    // Free any previous result!
    stbds_arrfree(chainValidation.exposedInfo);
    chainValidation.exposedInfo = nullptr;

    // Easy case, no chain...
    if (!chain)
      return inputType;

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
        !detached ? consumables : nullptr); // detached don't share context!

    return passthrough ? inputType : chainValidation.outputType;
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
      chain = value.payload.chainValue;
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
      return Var(chain);
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

struct ChainRunner : public ChainBase {
  // Only chain runners should expose varaibles to the context
  CBExposedTypesInfo exposedVariables() {
    return !detached ? chainValidation.exposedInfo : nullptr;
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
    case 1:
      return Var(once);
    case 2:
      return Var(passthrough);
    case 3:
      return Var(detached);
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

      if (detached) {
        if (!chainblocks::isRunning(chain)) {
          context->chain->node->schedule(chain, input);
        }
        return input;
      } else {
        // Run within the root flow
        auto runRes = runSubChain(chain, context, input);
        if (unlikely(runRes.state == Failed || context->aborted)) {
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
};

struct Dispatch : public ChainRunner {
  // Always passes back the input as output

  Dispatch() : ChainRunner() {
    passthrough = true;
    once = false;
    detached = false;
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(chainOnlyParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      chain = value.payload.chainValue;
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
    default:
      break;
    }
    return Var();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    // Run within the root flow
    auto runRes = runSubChain(chain, context, input);
    if (unlikely(runRes.state == Failed || context->aborted)) {
      return Var::Stop();
    }
    // Pass back the input
    return input;
  }
};

struct DispatchOnce : public ChainRunner {
  // Always passes back the input as output

  DispatchOnce() : ChainRunner() {
    passthrough = true;
    once = true;
    detached = false;
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(chainOnlyParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      chain = value.payload.chainValue;
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
    default:
      break;
    }
    return Var();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    if (!doneOnce) {
      doneOnce = true;

      // Run within the root flow
      auto runRes = runSubChain(chain, context, input);
      if (unlikely(runRes.state == Failed || context->aborted)) {
        return Var::Stop();
      }
    }
    return input;
  }
};

struct Do : public ChainRunner {
  // Always passes as output the actual inner chain output

  Do() : ChainRunner() {
    passthrough = false;
    once = false;
    detached = false;
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(chainOnlyParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      chain = value.payload.chainValue;
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
    default:
      break;
    }
    return Var();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    // Run within the root flow
    auto runRes = runSubChain(chain, context, input);
    if (unlikely(runRes.state == Failed || context->aborted)) {
      return Var::Stop();
    }
    // Pass the actual output
    return runRes.output;
  }
};

struct DoOnce : public ChainRunner {
  // Always passes as output the actual inner chain output, once, other times
  // it's a passthru

  DoOnce() : ChainRunner() {
    passthrough = false;
    once = true;
    detached = false;
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(chainOnlyParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      chain = value.payload.chainValue;
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
    default:
      break;
    }
    return Var();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    if (!doneOnce) {
      doneOnce = true;

      // Run within the root flow
      auto runRes = runSubChain(chain, context, input);
      if (unlikely(runRes.state == Failed || context->aborted)) {
        return Var::Stop();
      }
      return runRes.output;
    } else {
      return input;
    }
  }
};

struct Detach : public ChainRunner {
  // Detaches completely into a new chain on the same node
  // it's a passthru

  Detach() : ChainRunner() {
    passthrough = true;
    once = false;
    detached = true;
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(chainOnlyParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      chain = value.payload.chainValue;
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
    default:
      break;
    }
    return Var();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    assert(context && context->chain && context->chain->node);

    if (!chainblocks::isRunning(chain)) {
      context->chain->node->schedule(chain, input);
    }
    return input;
  }
};

struct DetachOnce : public ChainRunner {
  // Detaches completely into a new chain on the same node
  // it's a passthru

  DetachOnce() : ChainRunner() {
    passthrough = true;
    once = true;
    detached = true;
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(chainOnlyParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      chain = value.payload.chainValue;
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
    default:
      break;
    }
    return Var();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    assert(context && context->chain && context->chain->node);

    if (!doneOnce) {
      doneOnce = true;
      if (!chainblocks::isRunning(chain)) {
        context->chain->node->schedule(chain, input);
      }
    }
    return input;
  }
};

struct ChainLoadResult {
  bool hasError;
  std::string errorMsg;
  CBChain *chain;
};

struct ChainFileWatcher {
  std::atomic_bool running;
  std::thread worker;
  std::string fileName;
  rigtorp::SPSCQueue<ChainLoadResult> results;

  explicit ChainFileWatcher(std::string &file)
      : running(true), fileName(file), results(2) {
    worker = std::thread([this] {
      std::time_t lastWrite = 0;
      while (running) {
        try {
          bfs::path p(fileName);
          if (bfs::exists(p) && bfs::is_regular_file(p) &&
              bfs::last_write_time(p) > lastWrite) {
            if (!p.is_absolute()) {
              p = bfs::absolute(p); // from current
            }

            std::ifstream jf(p.c_str());
            json j;
            jf >> j;
            auto chain = j.get<CBChain *>();
            ChainLoadResult result = {false, "", chain};
            results.push(result);

            // make sure to store last write time and compare
            auto writeTime = bfs::last_write_time(p);
            lastWrite = writeTime;
          }
          // sleep some
          chainblocks::sleep(2.0);
        } catch (std::exception &e) {
          ChainLoadResult result = {true, e.what(), nullptr};
          results.push(result);
        } catch (...) {
          ChainLoadResult result = {true, "general error", nullptr};
          results.push(result);
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

  ChainLoader() : ChainRunner(), watcher(nullptr) {}

  static CBParametersInfo parameters() {
    return CBParametersInfo(chainloaderParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      fileName = value.payload.stringValue;
      watcher.reset(new ChainFileWatcher(fileName));
      break;
    case 1:
      once = value.payload.boolValue;
      break;
    case 2:
      detached = value.payload.boolValue;
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
      return Var(detached);
      break;
    default:
      break;
    }
    return Var();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!watcher->results.empty()) {
      auto result = watcher->results.front();
      if (unlikely(result->hasError)) {
        LOG(ERROR) << "Failed to reload a chain via ChainLoader, reason: "
                   << result->errorMsg;
      } else {
        if (chain) {
          chainblocks::stop(chain);
          delete chain;
        }
        chain = result->chain;
      }
      watcher->results.pop();
    }

    if (unlikely(!chain))
      return input;

    if (!doneOnce) {
      if (once)
        doneOnce = true;

      if (detached) {
        if (!chainblocks::isRunning(chain)) {
          context->chain->node->schedule(chain, input);
        }
        return input;
      } else {
        // Run within the root flow
        auto runRes = runSubChain(chain, context, input);
        if (unlikely(runRes.state == Failed || context->aborted)) {
          return Var::Stop();
        } else {
          return input;
        }
      }
    } else {
      return input;
    }
  }
};

#define CHAIN_BLOCK_DEF(NAME)                                                  \
  RUNTIME_CORE_BLOCK(NAME);                                                    \
  RUNTIME_BLOCK_inputTypes(NAME);                                              \
  RUNTIME_BLOCK_outputTypes(NAME);                                             \
  RUNTIME_BLOCK_parameters(NAME);                                              \
  RUNTIME_BLOCK_inferTypes(NAME);                                              \
  RUNTIME_BLOCK_exposedVariables(NAME);                                        \
  RUNTIME_BLOCK_setParam(NAME);                                                \
  RUNTIME_BLOCK_getParam(NAME);                                                \
  RUNTIME_BLOCK_activate(NAME);                                                \
  RUNTIME_BLOCK_cleanup(NAME);                                                 \
  RUNTIME_BLOCK_destroy(NAME);                                                 \
  RUNTIME_BLOCK_END(NAME);

// Register
CHAIN_BLOCK_DEF(RunChain);
CHAIN_BLOCK_DEF(Do);
CHAIN_BLOCK_DEF(DoOnce);
CHAIN_BLOCK_DEF(Dispatch);
CHAIN_BLOCK_DEF(DispatchOnce);
CHAIN_BLOCK_DEF(DetachOnce);
CHAIN_BLOCK_DEF(Detach);
CHAIN_BLOCK_DEF(ChainLoader);

RUNTIME_CORE_BLOCK(WaitChain);
RUNTIME_BLOCK_inputTypes(WaitChain);
RUNTIME_BLOCK_outputTypes(WaitChain);
RUNTIME_BLOCK_parameters(WaitChain);
RUNTIME_BLOCK_inferTypes(WaitChain);
RUNTIME_BLOCK_setParam(WaitChain);
RUNTIME_BLOCK_getParam(WaitChain);
RUNTIME_BLOCK_activate(WaitChain);
RUNTIME_BLOCK_cleanup(WaitChain);
RUNTIME_BLOCK_destroy(WaitChain);
RUNTIME_BLOCK_END(WaitChain);

void registerChainsBlocks() {
  REGISTER_CORE_BLOCK(RunChain);
  REGISTER_CORE_BLOCK(Do);
  REGISTER_CORE_BLOCK(DoOnce);
  REGISTER_CORE_BLOCK(Dispatch);
  REGISTER_CORE_BLOCK(DispatchOnce);
  REGISTER_CORE_BLOCK(Detach);
  REGISTER_CORE_BLOCK(DetachOnce);
  REGISTER_CORE_BLOCK(ChainLoader);
  REGISTER_CORE_BLOCK(WaitChain);
}
}; // namespace chainblocks

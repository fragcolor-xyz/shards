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

struct ChainRunner {
  CBChain *chain;
  bool once;
  bool doneOnce;
  bool passthrough;
  bool detached;

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

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
        auto runRes = chainblocks_RunSubChain(chain, context, input);
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
    auto runRes = chainblocks_RunSubChain(chain, context, input);
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
      auto runRes = chainblocks_RunSubChain(chain, context, input);
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
    auto runRes = chainblocks_RunSubChain(chain, context, input);
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
      auto runRes = chainblocks_RunSubChain(chain, context, input);
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
      watcher = std::make_unique<ChainFileWatcher>(fileName);
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
        auto runRes = chainblocks_RunSubChain(chain, context, input);
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

// Register
RUNTIME_CORE_BLOCK(RunChain);
RUNTIME_BLOCK_inputTypes(RunChain);
RUNTIME_BLOCK_outputTypes(RunChain);
RUNTIME_BLOCK_parameters(RunChain);
RUNTIME_BLOCK_inferTypes(RunChain);
RUNTIME_BLOCK_setParam(RunChain);
RUNTIME_BLOCK_getParam(RunChain);
RUNTIME_BLOCK_activate(RunChain);
RUNTIME_BLOCK_cleanup(RunChain);
RUNTIME_BLOCK_END(RunChain);

RUNTIME_CORE_BLOCK(Do);
RUNTIME_BLOCK_inputTypes(Do);
RUNTIME_BLOCK_outputTypes(Do);
RUNTIME_BLOCK_parameters(Do);
RUNTIME_BLOCK_inferTypes(Do);
RUNTIME_BLOCK_setParam(Do);
RUNTIME_BLOCK_getParam(Do);
RUNTIME_BLOCK_activate(Do);
RUNTIME_BLOCK_cleanup(Do);
RUNTIME_BLOCK_END(Do);

RUNTIME_CORE_BLOCK(DoOnce);
RUNTIME_BLOCK_inputTypes(DoOnce);
RUNTIME_BLOCK_outputTypes(DoOnce);
RUNTIME_BLOCK_parameters(DoOnce);
RUNTIME_BLOCK_inferTypes(DoOnce);
RUNTIME_BLOCK_setParam(DoOnce);
RUNTIME_BLOCK_getParam(DoOnce);
RUNTIME_BLOCK_activate(DoOnce);
RUNTIME_BLOCK_cleanup(DoOnce);
RUNTIME_BLOCK_END(DoOnce);

RUNTIME_CORE_BLOCK(Dispatch);
RUNTIME_BLOCK_inputTypes(Dispatch);
RUNTIME_BLOCK_outputTypes(Dispatch);
RUNTIME_BLOCK_parameters(Dispatch);
RUNTIME_BLOCK_setParam(Dispatch);
RUNTIME_BLOCK_getParam(Dispatch);
RUNTIME_BLOCK_activate(Dispatch);
RUNTIME_BLOCK_cleanup(Dispatch);
RUNTIME_BLOCK_END(Dispatch);

RUNTIME_CORE_BLOCK(DispatchOnce);
RUNTIME_BLOCK_inputTypes(DispatchOnce);
RUNTIME_BLOCK_outputTypes(DispatchOnce);
RUNTIME_BLOCK_parameters(DispatchOnce);
RUNTIME_BLOCK_setParam(DispatchOnce);
RUNTIME_BLOCK_getParam(DispatchOnce);
RUNTIME_BLOCK_activate(DispatchOnce);
RUNTIME_BLOCK_cleanup(DispatchOnce);
RUNTIME_BLOCK_END(DispatchOnce);

RUNTIME_CORE_BLOCK(DetachOnce);
RUNTIME_BLOCK_inputTypes(DetachOnce);
RUNTIME_BLOCK_outputTypes(DetachOnce);
RUNTIME_BLOCK_parameters(DetachOnce);
RUNTIME_BLOCK_setParam(DetachOnce);
RUNTIME_BLOCK_getParam(DetachOnce);
RUNTIME_BLOCK_activate(DetachOnce);
RUNTIME_BLOCK_cleanup(DetachOnce);
RUNTIME_BLOCK_END(DetachOnce);

RUNTIME_CORE_BLOCK(Detach);
RUNTIME_BLOCK_inputTypes(Detach);
RUNTIME_BLOCK_outputTypes(Detach);
RUNTIME_BLOCK_parameters(Detach);
RUNTIME_BLOCK_setParam(Detach);
RUNTIME_BLOCK_getParam(Detach);
RUNTIME_BLOCK_activate(Detach);
RUNTIME_BLOCK_cleanup(Detach);
RUNTIME_BLOCK_END(Detach);

RUNTIME_CORE_BLOCK(ChainLoader);
RUNTIME_BLOCK_inputTypes(ChainLoader);
RUNTIME_BLOCK_outputTypes(ChainLoader);
RUNTIME_BLOCK_parameters(ChainLoader);
RUNTIME_BLOCK_setParam(ChainLoader);
RUNTIME_BLOCK_getParam(ChainLoader);
RUNTIME_BLOCK_activate(ChainLoader);
RUNTIME_BLOCK_cleanup(ChainLoader);
RUNTIME_BLOCK_END(ChainLoader);

void registerChainsBlocks() {
  REGISTER_CORE_BLOCK(RunChain);
  REGISTER_CORE_BLOCK(Do);
  REGISTER_CORE_BLOCK(DoOnce);
  REGISTER_CORE_BLOCK(Dispatch);
  REGISTER_CORE_BLOCK(DispatchOnce);
  REGISTER_CORE_BLOCK(Detach);
  REGISTER_CORE_BLOCK(DetachOnce);
  REGISTER_CORE_BLOCK(ChainLoader);
}
}; // namespace chainblocks

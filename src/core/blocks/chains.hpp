#pragma once

#include "3rdparty/SPSCQueue.h"
#include "shared.hpp"
#include <boost/filesystem.hpp>
#include <chrono>
#include <fstream>
#include <thread>

namespace bfs = boost::filesystem;

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

static ParamsInfo chainloaderParamsInfo = ParamsInfo(
    ParamsInfo::Param("File", "The json file of the chain to run and watch.",
                      CBTypesInfo(strInfo)),
    ParamsInfo::Param("Once",
                      "Runs this sub-chain only once within the parent chain "
                      "execution cycle.",
                      CBTypesInfo(boolInfo)),
    ParamsInfo::Param("Detached",
                      "Runs the sub-chain as a completely separate parallel "
                      "chain in the same node.",
                      CBTypesInfo(boolInfo)));

template <typename T>
static inline CBVar activateRunChain(T *block, CBChain *chain,
                                     CBContext *context, CBVar input) {
  if (!chain)
    return input;

  if (!block->doneOnce) {
    if (block->once)
      block->doneOnce = true;

    if (block->detached) {
      if (!chainblocks::isRunning(chain)) {
        chainblocks::CurrentChain->node->schedule(chain, input);
      }
      return input;
    } else {
      // Run within the root flow, just call runChain

      chain->finished = false; // Reset finished flag (atomic)
      auto runRes = runChain(chain, context, input);
      chain->finishedOutput = runRes.output; // Write result before setting flag
      chain->finished = true;                // Set finished flag (atomic)
      if (runRes.state == Failed || context->aborted) {
        return Var::Stop();
      } else if (block->passthrough) {
        return input;
      } else {
        return runRes.output;
      }
    }
  } else {
    return input;
  }
}

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
    if (unlikely(!chain))
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

  void cleanup() {
    if (chain)
      chainblocks::stop(chain);
    doneOnce = false;
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

  ChainFileWatcher(std::string &file)
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

struct ChainLoader {
  std::string fileName;
  CBChain *chain;
  bool once;
  bool doneOnce;
  bool detached;
  std::unique_ptr<ChainFileWatcher> watcher;

  ChainLoader()
      : chain(nullptr), once(false), doneOnce(false), detached(false),
        watcher(nullptr) {}

  CBTypesInfo inputTypes() { return CBTypesInfo(anyInfo); }

  CBTypesInfo outputTypes() { return CBTypesInfo(noneInfo); }

  CBParametersInfo parameters() {
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

  CBVar activate(CBContext *context, CBVar input) {
    if (!watcher.get()->results.empty()) {
      auto result = watcher.get()->results.front();
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
      watcher.get()->results.pop();
    }

    if (unlikely(!chain))
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

  void cleanup() {
    if (chain)
      chainblocks::stop(chain);
    doneOnce = false;
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

RUNTIME_CORE_BLOCK(ChainLoader);
RUNTIME_BLOCK_inputTypes(ChainLoader);
RUNTIME_BLOCK_outputTypes(ChainLoader);
RUNTIME_BLOCK_parameters(ChainLoader);
RUNTIME_BLOCK_setParam(ChainLoader);
RUNTIME_BLOCK_getParam(ChainLoader);
RUNTIME_BLOCK_activate(ChainLoader);
RUNTIME_BLOCK_cleanup(ChainLoader);
RUNTIME_BLOCK_END(ChainLoader);
}; // namespace chainblocks

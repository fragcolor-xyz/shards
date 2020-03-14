/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifndef CB_RUNTIME_HPP
#define CB_RUNTIME_HPP

// ONLY CLANG AND GCC SUPPORTED FOR NOW

#include <string.h> // memset

#include "blocks_macros.hpp"
#include "core.hpp"
// C++ Mandatory from now!

// Since we build the runtime we are free to use any std and lib
#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double>;

// For sleep
#if _WIN32
#include <Windows.h>
#else
#include <time.h>
#endif

void freeDerivedInfo(CBTypeInfo info);
CBTypeInfo deriveTypeInfo(CBVar &value);

[[nodiscard]] CBValidationResult
validateConnections(const std::vector<CBlock *> &chain,
                    CBValidationCallback callback, void *userData,
                    CBInstanceData data, bool globalsOnly);
[[nodiscard]] CBValidationResult
validateConnections(const CBlocks chain, CBValidationCallback callback,
                    void *userData, CBInstanceData data = CBInstanceData());
[[nodiscard]] CBValidationResult
validateConnections(const CBChain *chain, CBValidationCallback callback,
                    void *userData, CBInstanceData data = CBInstanceData());

bool validateSetParam(CBlock *block, int index, CBVar &value,
                      CBValidationCallback callback, void *userData);

struct CBContext {
  CBContext(CBCoro &&sink, CBChain *starter)
      : main(starter), restarted(false), aborted(false),
        continuation(std::move(sink)), iterationCount(0), stack({}) {
    chainStack.push_back(starter);
  }

  ~CBContext() { chainblocks::arrayFree(stack); }

  const CBChain *main;
  std::vector<CBChain *> chainStack;

  // Those 2 go together with CBVar chainstates restart and stop
  bool restarted;
  // Also used to cancel a chain
  bool aborted;

  // Used within the coro& stack! (suspend, etc)
  CBCoro &&continuation;
  Duration next{};

  // Iteration counter
  uint64_t iterationCount;

  // Stack for local vars
  CBSeq stack;
};

#include "blocks/core.hpp"
#include "blocks/math.hpp"

namespace chainblocks {

void installSignalHandlers();

ALWAYS_INLINE inline CBVar activateBlock(CBlock *blk, CBContext *context,
                                         const CBVar &input) {
  switch (blk->inlineBlockId) {
  case StackPush: {
    chainblocks::arrayPush(context->stack, input);
    return input;
  }
  case StackPop: {
    if (context->stack.len == 0)
      throw CBException("Pop: Stack underflow!");
    return chainblocks::arrayPop<CBSeq, CBVar>(context->stack);
  }
  case StackSwap: {
    auto s = context->stack.len;
    auto a = context->stack.elements[s - 1];
    context->stack.elements[s - 1] = context->stack.elements[s - 2];
    context->stack.elements[s - 2] = a;
    return input;
  }
  case StackDrop: {
    if (context->stack.len == 0)
      throw CBException("Drop: Stack underflow!");
    context->stack.len--;
    return input;
  }
  case CoreConst: {
    auto cblock = reinterpret_cast<chainblocks::ConstRuntime *>(blk);
    return cblock->core._value;
  }
  case CoreIs: {
    auto cblock = reinterpret_cast<chainblocks::IsRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreIsNot: {
    auto cblock = reinterpret_cast<chainblocks::IsNotRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreAnd: {
    auto cblock = reinterpret_cast<chainblocks::AndRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreOr: {
    auto cblock = reinterpret_cast<chainblocks::OrRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreNot: {
    auto cblock = reinterpret_cast<chainblocks::NotRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreIsMore: {
    auto cblock = reinterpret_cast<chainblocks::IsMoreRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreIsLess: {
    auto cblock = reinterpret_cast<chainblocks::IsLessRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreIsMoreEqual: {
    auto cblock = reinterpret_cast<chainblocks::IsMoreEqualRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreIsLessEqual: {
    auto cblock = reinterpret_cast<chainblocks::IsLessEqualRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreSleep: {
    auto cblock = reinterpret_cast<chainblocks::PauseRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreInput: {
    auto cblock = reinterpret_cast<chainblocks::InputRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreStop: {
    auto cblock = reinterpret_cast<chainblocks::StopRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreRestart: {
    auto cblock = reinterpret_cast<chainblocks::RestartRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreTakeSeq: {
    auto cblock = reinterpret_cast<chainblocks::TakeRuntime *>(blk);
    return cblock->core.activateSeq(context, input);
  }
  case CoreTakeFloats: {
    auto cblock = reinterpret_cast<chainblocks::TakeRuntime *>(blk);
    return cblock->core.activateFloats(context, input);
  }
  case CorePush: {
    auto cblock = reinterpret_cast<chainblocks::PushRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreRepeat: {
    auto cblock = reinterpret_cast<chainblocks::RepeatRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreGet: {
    auto cblock = reinterpret_cast<chainblocks::GetRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreSet: {
    auto cblock = reinterpret_cast<chainblocks::SetRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreUpdate: {
    auto cblock = reinterpret_cast<chainblocks::UpdateRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreSwap: {
    auto cblock = reinterpret_cast<chainblocks::SwapRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathAdd: {
    auto cblock = reinterpret_cast<chainblocks::Math::AddRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathSubtract: {
    auto cblock = reinterpret_cast<chainblocks::Math::SubtractRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathMultiply: {
    auto cblock = reinterpret_cast<chainblocks::Math::MultiplyRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathDivide: {
    auto cblock = reinterpret_cast<chainblocks::Math::DivideRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathXor: {
    auto cblock = reinterpret_cast<chainblocks::Math::XorRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathAnd: {
    auto cblock = reinterpret_cast<chainblocks::Math::AndRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathOr: {
    auto cblock = reinterpret_cast<chainblocks::Math::OrRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathMod: {
    auto cblock = reinterpret_cast<chainblocks::Math::ModRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathLShift: {
    auto cblock = reinterpret_cast<chainblocks::Math::LShiftRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathRShift: {
    auto cblock = reinterpret_cast<chainblocks::Math::RShiftRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathAbs: {
    auto cblock = reinterpret_cast<chainblocks::Math::AbsRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
#if 1
  case MathExp: {
    auto cblock = reinterpret_cast<chainblocks::Math::ExpRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathExp2: {
    auto cblock = reinterpret_cast<chainblocks::Math::Exp2Runtime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathExpm1: {
    auto cblock = reinterpret_cast<chainblocks::Math::Expm1Runtime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathLog: {
    auto cblock = reinterpret_cast<chainblocks::Math::LogRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathLog10: {
    auto cblock = reinterpret_cast<chainblocks::Math::Log10Runtime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathLog2: {
    auto cblock = reinterpret_cast<chainblocks::Math::Log2Runtime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathLog1p: {
    auto cblock = reinterpret_cast<chainblocks::Math::Log1pRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathSqrt: {
    auto cblock = reinterpret_cast<chainblocks::Math::SqrtRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathCbrt: {
    auto cblock = reinterpret_cast<chainblocks::Math::CbrtRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathSin: {
    auto cblock = reinterpret_cast<chainblocks::Math::SinRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathCos: {
    auto cblock = reinterpret_cast<chainblocks::Math::CosRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathTan: {
    auto cblock = reinterpret_cast<chainblocks::Math::TanRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathAsin: {
    auto cblock = reinterpret_cast<chainblocks::Math::AsinRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathAcos: {
    auto cblock = reinterpret_cast<chainblocks::Math::AcosRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathAtan: {
    auto cblock = reinterpret_cast<chainblocks::Math::AtanRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathSinh: {
    auto cblock = reinterpret_cast<chainblocks::Math::SinhRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathCosh: {
    auto cblock = reinterpret_cast<chainblocks::Math::CoshRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathTanh: {
    auto cblock = reinterpret_cast<chainblocks::Math::TanhRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathAsinh: {
    auto cblock = reinterpret_cast<chainblocks::Math::AsinhRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathAcosh: {
    auto cblock = reinterpret_cast<chainblocks::Math::AcoshRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathAtanh: {
    auto cblock = reinterpret_cast<chainblocks::Math::AtanhRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathErf: {
    auto cblock = reinterpret_cast<chainblocks::Math::ErfRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathErfc: {
    auto cblock = reinterpret_cast<chainblocks::Math::ErfcRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathTGamma: {
    auto cblock = reinterpret_cast<chainblocks::Math::TGammaRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathLGamma: {
    auto cblock = reinterpret_cast<chainblocks::Math::LGammaRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
#endif
  case MathCeil: {
    auto cblock = reinterpret_cast<chainblocks::Math::CeilRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathFloor: {
    auto cblock = reinterpret_cast<chainblocks::Math::FloorRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathTrunc: {
    auto cblock = reinterpret_cast<chainblocks::Math::TruncRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case MathRound: {
    auto cblock = reinterpret_cast<chainblocks::Math::RoundRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  default: {
    // NotInline
    return blk->activate(blk, context, &input);
  }
  }
}

CBRunChainOutput runChain(CBChain *chain, CBContext *context,
                          const CBVar &chainInput);

inline CBRunChainOutput runSubChain(CBChain *chain, CBContext *context,
                                    const CBVar &input) {
  // push to chain stack
  context->chainStack.push_back(chain);
  auto runRes = chainblocks::runChain(chain, context, input);
  // Write result before setting flag, TODO mutex inter-locked?
  chain->finishedOutput = runRes.output;
  // restore stack chain
  context->chainStack.pop_back();
  return runRes;
}

inline void cleanup(CBChain *chain) {
  // Run cleanup on all blocks, prepare them for a new start if necessary
  // Do this in reverse to allow a safer cleanup
  for (auto it = chain->blocks.rbegin(); it != chain->blocks.rend(); ++it) {
    auto blk = *it;
    try {
      blk->cleanup(blk);
    } catch (boost::context::detail::forced_unwind const &e) {
      throw; // required for Boost Coroutine!
    } catch (const std::exception &e) {
      LOG(ERROR) << "Block cleanup error, failed block: "
                 << std::string(blk->name(blk));
      LOG(ERROR) << e.what() << '\n';
    } catch (...) {
      LOG(ERROR) << "Block cleanup error, failed block: "
                 << std::string(blk->name(blk));
    }
  }
  // Also clear all variables reporting dangling ones
  for (auto var : chain->variables) {
    if (var.second.refcount > 0) {
      LOG(ERROR) << "Found a dangling variable: " << var.first
                 << " in chain: " << chain->name;
    }
  }
  chain->variables.clear();
}

boost::context::continuation run(CBChain *chain,
                                 boost::context::continuation &&sink);

inline void prepare(CBChain *chain) {
  if (chain->coro)
    return;

  chain->coro = new CBCoro(
      boost::context::callcc([&chain](boost::context::continuation &&sink) {
        return run(chain, std::move(sink));
      }));
}

inline void start(CBChain *chain, CBVar input = {}) {
  if (chain->state != CBChain::State::Prepared) {
    LOG(ERROR) << "Attempted to start a chain not ready for running!";
    return;
  }

  if (!chain->coro || !(*chain->coro))
    return; // check if not null and bool operator also to see if alive!

  chainblocks::cloneVar(chain->rootTickInput, input);
  chain->state = CBChain::State::Starting;
}

inline bool stop(CBChain *chain, CBVar *result = nullptr) {
  // Clone the results if we need them
  if (result)
    cloneVar(*result, chain->finishedOutput);

  if (chain->coro) {
    // Run until exit if alive, need to propagate to all suspended blocks!
    if ((*chain->coro) && chain->state > CBChain::State::Stopped &&
        chain->state < CBChain::State::Failed) {
      // set abortion flag, we always have a context in this case
      chain->context->aborted = true;

      // BIG Warning: chain->context existed in the coro stack!!!
      // after this resume chain->context is trash!
      chain->coro->resume();
    }

    // delete also the coro ptr
    delete chain->coro;
    chain->coro = nullptr;
  } else {
    // if we had a coro this will run inside it!
    cleanup(chain);
  }

  // return true if we ended, as in we did our job
  auto res = chain->state == CBChain::State::Ended;

  chain->state = CBChain::State::Stopped;

  return res;
}

inline bool isRunning(CBChain *chain) {
  return chain->state >= CBChain::State::Starting &&
         chain->state <= CBChain::State::IterationEnded;
}

inline bool tick(CBChain *chain, CBVar rootInput = {}) {
  if (!chain->context || !chain->coro || !(*chain->coro) || !(isRunning(chain)))
    return false; // check if not null and bool operator also to see if alive!

  Duration now = Clock::now().time_since_epoch();
  if (now >= chain->context->next) {
    if (rootInput != Empty) {
      cloneVar(chain->rootTickInput, rootInput);
    }
    *chain->coro = chain->coro->resume();
  }
  return true;
}

inline bool hasEnded(CBChain *chain) {
  return chain->state > CBChain::State::IterationEnded;
}

inline bool isCanceled(CBContext *context) { return context->aborted; }

inline void sleep(double seconds = -1.0, bool runCallbacks = true) {
  // negative = no sleep, just run callbacks

  // Run callbacks first if needed
  // Take note of how long it took and subtract from sleep time! if some time is
  // left sleep
  if (runCallbacks) {
    Duration sleepTime(seconds);
    auto pre = Clock::now();
    for (auto &cbinfo : Globals::RunLoopHooks) {
      if (cbinfo.second) {
        cbinfo.second();
      }
    }
    auto post = Clock::now();

    Duration cbsTime = post - pre;
    Duration realSleepTime = sleepTime - cbsTime;
    if (realSleepTime.count() > 0.0) {
      // Sleep actual time minus stuff we did in cbs
      seconds = realSleepTime.count();
    } else {
      // Don't yield to kernel at all in this case!
      seconds = 1.0;
    }
  }

  if (seconds >= 0.0) {
#ifdef _WIN32
    HANDLE timer;
    LARGE_INTEGER ft;
    ft.QuadPart = -(int64_t(seconds * 10000000));
    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
#else
    struct timespec delay;
    seconds += 0.5e-9; // add half epsilon
    delay.tv_sec = (decltype(delay.tv_sec))seconds;
    delay.tv_nsec = (seconds - delay.tv_sec) * 1000000000L;
    while (nanosleep(&delay, &delay))
      (void)0;
#endif
  }
}

struct RuntimeCallbacks {
  // TODO, turn them into filters maybe?
  virtual void registerBlock(const char *fullName,
                             CBBlockConstructor constructor) = 0;
  virtual void registerObjectType(int32_t vendorId, int32_t typeId,
                                  CBObjectInfo info) = 0;
  virtual void registerEnumType(int32_t vendorId, int32_t typeId,
                                CBEnumInfo info) = 0;
};
}; // namespace chainblocks

struct CBNode {
  ~CBNode() { terminate(); }

  void schedule(CBChain *chain, CBVar input = {}, bool validate = true) {
    if (chain->node)
      throw chainblocks::CBException(
          "schedule failed, chain was already scheduled!");

    // Validate the chain
    if (validate) {
      auto validation = validateConnections(
          chain->blocks,
          [](const CBlock *errorBlock, const char *errorTxt,
             bool nonfatalWarning, void *userData) {
            auto node = reinterpret_cast<CBNode *>(userData);
            auto blk = const_cast<CBlock *>(errorBlock);
            if (!nonfatalWarning) {
              node->errorMsg.assign(errorTxt);
              node->errorMsg += ", input block: " + std::string(blk->name(blk));
              throw chainblocks::CBException(node->errorMsg.c_str());
            } else {
              LOG(INFO) << "Validation warning: " << errorTxt
                        << " input block: " << blk->name(blk);
            }
          },
          this, CBInstanceData(), true);
      chainblocks::arrayFree(validation.exposedInfo);
    }

    auto flow = std::make_shared<CBFlow>();
    flow->chain = chain;
    flows.push_back(flow);
    chain->node = this;
    chain->flow = flow.get();
    chainblocks::prepare(chain);
    chainblocks::start(chain, input);
  }

  bool tick(CBVar input = Empty) {
    auto noErrors = true;
    _runningFlows = flows;
    for (auto &flow : _runningFlows) {
      // make sure flow is actually the current one
      // since this chain might be moved into another flow!
      if (flow.get() != flow->chain->flow)
        continue;

      chainblocks::tick(flow->chain, input);
      if (!chainblocks::isRunning(flow->chain)) {
        if (!chainblocks::stop(flow->chain)) {
          noErrors = false;
        }
        flows.remove(flow);
        flow->chain->node = nullptr;
        flow->chain->flow = nullptr;
      }
    }
    return noErrors;
  }

  void terminate() {
    for (auto &flow : flows) {
      chainblocks::stop(flow->chain);
      flow->chain->node = nullptr;
      flow->chain->flow = nullptr;
    }
    flows.clear();
  }

  void remove(CBChain *chain) {
    chainblocks::stop(chain);
    flows.remove_if([chain](const std::shared_ptr<CBFlow> &flow) {
      return flow->chain == chain;
    });
    chain->node = nullptr;
    chain->flow = nullptr;
  }

  bool empty() { return flows.empty(); }

  std::unordered_map<std::string, CBVar, std::hash<std::string>,
                     std::equal_to<std::string>,
                     boost::alignment::aligned_allocator<
                         std::pair<const std::string, CBVar>, 16>>
      variables;
  std::list<std::shared_ptr<CBFlow>> flows;
  std::list<std::shared_ptr<CBFlow>> _runningFlows;
  std::string errorMsg;
};

#endif

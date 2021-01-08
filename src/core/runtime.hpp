/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifndef CB_RUNTIME_HPP
#define CB_RUNTIME_HPP

// ONLY CLANG AND GCC SUPPORTED FOR NOW

#include <string.h> // memset

#include "blocks_macros.hpp"
#include "foundation.hpp"
// C++ Mandatory from now!

// Since we build the runtime we are free to use any std and lib
#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using CBClock = std::chrono::high_resolution_clock;
using CBDuration = std::chrono::duration<double>;

// For sleep
#if _WIN32
#include <Windows.h>
#else
#include <time.h>
#endif

#define CB_SUSPEND(_ctx_, _secs_)                                              \
  const auto _suspend_state = chainblocks::suspend(_ctx_, _secs_);             \
  if (_suspend_state != CBChainState::Continue)                                \
  return Var::Empty

struct CBContext {
  CBContext(
#ifndef __EMSCRIPTEN__
      CBCoro &&sink,
#else
      CBCoro *coro,
#endif
      const CBChain *starter, CBFlow *flow)
      : main(starter), flow(flow),
#ifndef __EMSCRIPTEN__
        continuation(std::move(sink))
#else
        continuation(coro)
#endif
  {
    chainStack.push_back(const_cast<CBChain *>(starter));
  }

  const CBChain *main;
  CBFlow *flow;
  std::vector<CBChain *> chainStack;
  bool onCleanup{false};

// Used within the coro& stack! (suspend, etc)
#ifndef __EMSCRIPTEN__
  CBCoro &&continuation;
#else
  CBCoro *continuation{nullptr};
#endif
  CBDuration next{};
#ifdef CB_USE_TSAN
  void *tsan_handle = nullptr;
#endif

  constexpr void stopFlow(const CBVar &lastValue) {
    state = CBChainState::Stop;
    flowStorage = lastValue;
  }

  constexpr void restartFlow(const CBVar &lastValue) {
    state = CBChainState::Restart;
    flowStorage = lastValue;
  }

  constexpr void returnFlow(const CBVar &lastValue) {
    state = CBChainState::Return;
    flowStorage = lastValue;
  }

  void cancelFlow(std::string_view message) {
    state = CBChainState::Stop;
    errorMessage = message;
    hasError = true;
  }

  void resetCancelFlow() {
    state = CBChainState::Continue;
    hasError = false;
  }

  constexpr void rebaseFlow() { state = CBChainState::Rebase; }

  constexpr void continueFlow() { state = CBChainState::Continue; }

  constexpr bool shouldContinue() const {
    return state == CBChainState::Continue;
  }

  constexpr bool shouldReturn() const { return state == CBChainState::Return; }

  constexpr bool shouldStop() const { return state == CBChainState::Stop; }

  constexpr bool failed() const { return hasError; }

  constexpr const std::string &getErrorMessage() { return errorMessage; }

  constexpr CBChainState getState() const { return state; }

  constexpr CBVar getFlowStorage() const { return flowStorage; }

private:
  CBChainState state = CBChainState::Continue;
  // Used when flow is stopped/restart/return
  // to store the previous result
  CBVar flowStorage{};
  bool hasError{false};
  std::string errorMessage;
};

namespace chainblocks {
void freeDerivedInfo(CBTypeInfo info);
CBTypeInfo deriveTypeInfo(const CBVar &value);

uint64_t deriveTypeHash(const CBVar &value);
uint64_t deriveTypeHash(const CBTypeInfo &value);

struct ToTypeInfo {
  ToTypeInfo(const CBVar &var) { _info = deriveTypeInfo(var); }

  ~ToTypeInfo() { freeDerivedInfo(_info); }

  operator const CBTypeInfo &() { return _info; }

  const CBTypeInfo *operator->() const { return &_info; }

private:
  CBTypeInfo _info;
};

[[nodiscard]] CBComposeResult composeChain(const std::vector<CBlock *> &chain,
                                           CBValidationCallback callback,
                                           void *userData, CBInstanceData data);
[[nodiscard]] CBComposeResult composeChain(const CBlocks chain,
                                           CBValidationCallback callback,
                                           void *userData, CBInstanceData data);
[[nodiscard]] CBComposeResult composeChain(const CBSeq chain,
                                           CBValidationCallback callback,
                                           void *userData, CBInstanceData data);
[[nodiscard]] CBComposeResult composeChain(const CBChain *chain,
                                           CBValidationCallback callback,
                                           void *userData, CBInstanceData data);

bool validateSetParam(CBlock *block, int index, const CBVar &value,
                      CBValidationCallback callback, void *userData);
} // namespace chainblocks

#include "blocks/core.hpp"
#include "blocks/math.hpp"
namespace chainblocks {

void installSignalHandlers();

FLATTEN ALWAYS_INLINE inline CBVar
activateBlock(CBlock *blk, CBContext *context, const CBVar &input) {
  switch (blk->inlineBlockId) {
  case NoopBlock:
    return input;
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
    auto cblock = reinterpret_cast<chainblocks::BlockWrapper<Pause> *>(blk);
    return cblock->block.activate(context, input);
  }
  case CoreInput: {
    auto cblock = reinterpret_cast<chainblocks::BlockWrapper<Input> *>(blk);
    return cblock->block.activate(context, input);
  }
  case CoreTakeSeq: {
    auto cblock = reinterpret_cast<chainblocks::TakeRuntime *>(blk);
    return cblock->core.activateSeq(context, input);
  }
  case CoreTakeFloats: {
    auto cblock = reinterpret_cast<chainblocks::TakeRuntime *>(blk);
    return cblock->core.activateFloats(context, input);
  }
  case CoreTakeTable: {
    auto cblock = reinterpret_cast<chainblocks::TakeRuntime *>(blk);
    return cblock->core.activateTable(context, input);
  }
  case CorePush: {
    auto cblock = reinterpret_cast<chainblocks::PushRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreRepeat: {
    auto cblock = reinterpret_cast<chainblocks::RepeatRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreOnce: {
    auto cblock = reinterpret_cast<chainblocks::BlockWrapper<Once> *>(blk);
    return cblock->block.activate(context, input);
  }
  case CoreGet: {
    auto cblock = reinterpret_cast<chainblocks::GetRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreSet: {
    auto cblock = reinterpret_cast<chainblocks::SetRuntime *>(blk);
    return cblock->core.activate(context, input);
  }
  case CoreRef: {
    auto cblock = reinterpret_cast<chainblocks::RefRuntime *>(blk);
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
#if 0
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
  DEFER({ context->chainStack.pop_back(); });
  auto runRes = chainblocks::runChain(chain, context, input);
  return runRes;
}

#ifndef __EMSCRIPTEN__
boost::context::continuation run(CBChain *chain, CBFlow *flow,
                                 boost::context::continuation &&sink);
#else
void run(CBChain *chain, CBFlow *flow, CBCoro *coro);
#endif

inline void prepare(CBChain *chain, CBFlow *flow) {
  if (chain->coro)
    return;

#ifdef CB_USE_TSAN
  auto curr = __tsan_get_current_fiber();
  if (chain->tsan_coro)
    __tsan_destroy_fiber(chain->tsan_coro);
  chain->tsan_coro = __tsan_create_fiber(0);
  __tsan_switch_to_fiber(chain->tsan_coro, 0);
#endif

#ifndef __EMSCRIPTEN__
  if (!chain->stack_mem) {
    chain->stack_mem = new (std::align_val_t{16}) uint8_t[CB_STACK_SIZE];
  }
  chain->coro = boost::context::callcc(
      std::allocator_arg, CBStackAllocator{chain->stack_mem},
      [chain, flow](boost::context::continuation &&sink) {
        return run(chain, flow, std::move(sink));
      });
#else
  chain->coro.emplace();
  chain->coro->init([=]() { run(chain, flow, &(*chain->coro)); });
  chain->coro->resume();
#endif

#ifdef CB_USE_TSAN
  __tsan_switch_to_fiber(curr, 0);
#endif
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

  for (auto &call : chain->onStart) {
    call();
  }
}

inline bool stop(CBChain *chain, CBVar *result = nullptr) {
  if (chain->state == CBChain::State::Stopped) {
    // Clone the results if we need them
    if (result)
      cloneVar(*result, chain->finishedOutput);

    return true;
  }

  LOG(TRACE) << "stopping chain: " << chain->name;

  if (chain->coro) {
    // Run until exit if alive, need to propagate to all suspended blocks!
    if ((*chain->coro) && chain->state > CBChain::State::Stopped &&
        chain->state < CBChain::State::Failed) {
      // set abortion flag, we always have a context in this case
      chain->context->stopFlow(Var::Empty);

      // BIG Warning: chain->context existed in the coro stack!!!
      // after this resume chain->context is trash!
#ifdef CB_USE_TSAN
      auto curr = __tsan_get_current_fiber();
      __tsan_switch_to_fiber(chain->tsan_coro, 0);
#endif

      chain->coro->resume();

#ifdef CB_USE_TSAN
      __tsan_switch_to_fiber(curr, 0);
#endif
    }

    // delete also the coro ptr
    chain->coro.reset();
  } else {
    // if we had a coro this will run inside it!
    chain->cleanup(true);
  }

  // return true if we ended, as in we did our job
  auto res = chain->state == CBChain::State::Ended;

  chain->state = CBChain::State::Stopped;
  destroyVar(chain->rootTickInput);

  // Clone the results if we need them
  if (result)
    cloneVar(*result, chain->finishedOutput);

  for (auto &call : chain->onStop) {
    call();
  }

  return res;
}

inline bool isRunning(CBChain *chain) {
  const auto state = chain->state.load(); // atomic
  return state >= CBChain::State::Starting &&
         state <= CBChain::State::IterationEnded;
}

inline bool tick(CBChain *chain, CBDuration now, CBVar rootInput = {}) {
  if (!chain->context || !chain->coro || !(*chain->coro) || !(isRunning(chain)))
    return false; // check if not null and bool operator also to see if alive!

  if (now >= chain->context->next) {
    if (rootInput != Var::Empty) {
      cloneVar(chain->rootTickInput, rootInput);
    }
#ifdef CB_USE_TSAN
    auto curr = __tsan_get_current_fiber();
    __tsan_switch_to_fiber(chain->tsan_coro, 0);
#endif

#ifndef __EMSCRIPTEN__
    *chain->coro = chain->coro->resume();
#else
    chain->coro->resume();
#endif

#ifdef CB_USE_TSAN
    __tsan_switch_to_fiber(curr, 0);
#endif
  }
  return true;
}

inline bool hasEnded(CBChain *chain) {
  return chain->state > CBChain::State::IterationEnded;
}

inline bool isCanceled(CBContext *context) { return context->shouldStop(); }

inline void sleep(double seconds = -1.0, bool runCallbacks = true) {
  // negative = no sleep, just run callbacks

  // Run callbacks first if needed
  // Take note of how long it took and subtract from sleep time! if some time is
  // left sleep
  if (runCallbacks) {
    CBDuration sleepTime(seconds);
    auto pre = CBClock::now();
    for (auto &cbinfo : Globals::RunLoopHooks) {
      if (cbinfo.second) {
        cbinfo.second();
      }
    }
    auto post = CBClock::now();

    CBDuration cbsTime = post - pre;
    CBDuration realSleepTime = sleepTime - cbsTime;
    if (seconds != -1.0 && realSleepTime.count() > 0.0) {
      // Sleep actual time minus stuff we did in cbs
      seconds = realSleepTime.count();
    } else {
      // Don't yield to kernel at all in this case!
      seconds = -1.0;
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
#elif __EMSCRIPTEN__
    unsigned int ms = floor(seconds * 1000.0);
    emscripten_sleep(ms);
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

using namespace chainblocks;

struct CBNode : public std::enable_shared_from_this<CBNode> {
  static std::shared_ptr<CBNode> make() {
    return std::shared_ptr<CBNode>(new CBNode());
  }

  static std::shared_ptr<CBNode> *makePtr() {
    return new std::shared_ptr<CBNode>(new CBNode());
  }

  ~CBNode() { terminate(); }

  struct EmptyObserver {
    void before_compose(CBChain *chain) {}
    void before_tick(CBChain *chain) {}
    void before_stop(CBChain *chain) {}
    void before_prepare(CBChain *chain) {}
    void before_start(CBChain *chain) {}
  };

  template <class Observer>
  void schedule(Observer observer, const std::shared_ptr<CBChain> &chain,
                CBVar input = {}, bool validate = true) {
    // TODO do this better...
    // this way does not work
    // if (chain->node.lock())
    //   throw chainblocks::CBException(
    //       "schedule failed, chain was already scheduled!");

    // this is to avoid recursion during compose
    visitedChains.clear();

    chain->node = shared_from_this();

    observer.before_compose(chain.get());
    if (validate) {
      // Validate the chain
      CBInstanceData data{};
      data.chain = chain.get();
      data.inputType = deriveTypeInfo(input);
      auto validation = composeChain(
          chain->blocks,
          [](const CBlock *errorBlock, const char *errorTxt,
             bool nonfatalWarning, void *userData) {
            auto blk = const_cast<CBlock *>(errorBlock);
            if (!nonfatalWarning) {
              throw ComposeError(std::string(errorTxt) + ", input block: " +
                                 std::string(blk->name(blk)));
            } else {
              LOG(INFO) << "Validation warning: " << errorTxt
                        << " input block: " << blk->name(blk);
            }
          },
          this, data);
      arrayFree(validation.exposedInfo);
      arrayFree(validation.requiredInfo);
      freeDerivedInfo(data.inputType);
    }

    observer.before_prepare(chain.get());
    // create a flow as well
    prepare(chain.get(), _flows.emplace_back(new CBFlow{chain.get()}).get());
    observer.before_start(chain.get());
    start(chain.get(), input);

    scheduled.insert(chain);
  }

  void schedule(const std::shared_ptr<CBChain> &chain, CBVar input = {},
                bool validate = true) {
    EmptyObserver obs;
    schedule(obs, chain, input, validate);
  }

  template <class Observer>
  bool tick(Observer observer, CBVar input = Var::Empty) {
    auto noErrors = true;
    _errors.clear();
    if (Globals::SigIntTerm > 0) {
      terminate();
    } else {
      CBDuration now = CBClock::now().time_since_epoch();
      for (auto it = _flows.begin(); it != _flows.end();) {
        auto &flow = *it;
        observer.before_tick(flow->chain);
        chainblocks::tick(flow->chain, now, input);
        if (unlikely(!isRunning(flow->chain))) {
          if (flow->chain->finishedError.size() > 0) {
            _errors.emplace_back(flow->chain->finishedError);
          }
          observer.before_stop(flow->chain);
          if (!stop(flow->chain)) {
            noErrors = false;
          }
          flow->chain->node.reset();
          it = _flows.erase(it);
        } else {
          ++it;
        }
      }
    }
    return noErrors;
  }

  bool tick(CBVar input = Var::Empty) {
    EmptyObserver obs;
    return tick(obs, input);
  }

  void terminate() {
    for (auto chain : scheduled) {
      stop(chain.get());
      chain->node.reset();
    }

    _flows.clear();

    // release all chains
    scheduled.clear();

    // find dangling variables and notice
    for (auto var : variables) {
      if (var.second.refcount > 0) {
        LOG(ERROR) << "Found a dangling global variable: " << var.first;
      }
    }
    variables.clear();
  }

  void remove(const std::shared_ptr<CBChain> &chain) {
    stop(chain.get());
    _flows.remove_if(
        [chain](auto &flow) { return flow->chain == chain.get(); });
    chain->node.reset();
    visitedChains.erase(chain.get());
    scheduled.erase(chain);
  }

  bool empty() { return _flows.empty(); }

  const std::vector<std::string> &errors() { return _errors; }

  std::unordered_map<std::string, CBVar, std::hash<std::string>,
                     std::equal_to<std::string>,
                     boost::alignment::aligned_allocator<
                         std::pair<const std::string, CBVar>, 16>>
      variables;

  std::unordered_map<CBChain *, CBTypeInfo> visitedChains;

  std::unordered_set<std::shared_ptr<CBChain>> scheduled;

private:
  std::list<std::shared_ptr<CBFlow>> _flows;
  std::vector<std::string> _errors;
  CBNode() = default;
};

namespace chainblocks {
struct Serialization {
  static void varFree(CBVar &output) {
    switch (output.valueType) {
    case CBType::None:
    case CBType::EndOfBlittableTypes:
    case CBType::Any:
    case CBType::Enum:
    case CBType::Bool:
    case CBType::Int:
    case CBType::Int2:
    case CBType::Int3:
    case CBType::Int4:
    case CBType::Int8:
    case CBType::Int16:
    case CBType::Float:
    case CBType::Float2:
    case CBType::Float3:
    case CBType::Float4:
    case CBType::Color:
      break;
    case CBType::Bytes:
      delete[] output.payload.bytesValue;
      break;
    case CBType::Array:
      chainblocks::arrayFree(output.payload.arrayValue);
      break;
    case CBType::Path:
    case CBType::String:
    case CBType::ContextVar: {
      delete[] output.payload.stringValue;
      break;
    }
    case CBType::Seq: {
      for (uint32_t i = 0; i < output.payload.seqValue.cap; i++) {
        varFree(output.payload.seqValue.elements[i]);
      }
      chainblocks::arrayFree(output.payload.seqValue);
      break;
    }
    case CBType::Table: {
      output.payload.tableValue.api->tableFree(output.payload.tableValue);
      break;
    }
    case CBType::Image: {
      delete[] output.payload.imageValue.data;
      break;
    }
    case CBType::Block: {
      auto blk = output.payload.blockValue;
      if (!blk->owned) {
        // destroy only if not owned
        blk->destroy(blk);
      }
      break;
    }
    case CBType::Chain: {
      CBChain::deleteRef(output.payload.chainValue);
      break;
    }
    case CBType::Object: {
      if ((output.flags & CBVAR_FLAGS_USES_OBJINFO) ==
              CBVAR_FLAGS_USES_OBJINFO &&
          output.objectInfo && output.objectInfo->release) {
        output.objectInfo->release(output.payload.objectValue);
      }
      break;
    }
    }

    memset(&output, 0x0, sizeof(CBVar));
  }

  std::unordered_map<std::string, CBChainRef> chains;
  std::unordered_map<std::string, std::shared_ptr<CBlock>> defaultBlocks;

  void reset() {
    for (auto &ref : chains) {
      CBChain::deleteRef(ref.second);
    }
    chains.clear();
    defaultBlocks.clear();
  }

  ~Serialization() { reset(); }

  template <class BinaryReader>
  void deserialize(BinaryReader &read, CBVar &output) {
    // we try to recycle memory so pass a empty None as first timer!
    CBType nextType;
    read((uint8_t *)&nextType, sizeof(output.valueType));

    // stop trying to recycle, types differ
    auto recycle = true;
    if (output.valueType != nextType) {
      varFree(output);
      recycle = false;
    }

    output.valueType = nextType;

    switch (output.valueType) {
    case CBType::None:
    case CBType::EndOfBlittableTypes:
    case CBType::Any:
      break;
    case CBType::Enum:
      read((uint8_t *)&output.payload, sizeof(int32_t) * 3);
      break;
    case CBType::Bool:
      read((uint8_t *)&output.payload, sizeof(CBBool));
      break;
    case CBType::Int:
      read((uint8_t *)&output.payload, sizeof(CBInt));
      break;
    case CBType::Int2:
      read((uint8_t *)&output.payload, sizeof(CBInt2));
      break;
    case CBType::Int3:
      read((uint8_t *)&output.payload, sizeof(CBInt3));
      break;
    case CBType::Int4:
      read((uint8_t *)&output.payload, sizeof(CBInt4));
      break;
    case CBType::Int8:
      read((uint8_t *)&output.payload, sizeof(CBInt8));
      break;
    case CBType::Int16:
      read((uint8_t *)&output.payload, sizeof(CBInt16));
      break;
    case CBType::Float:
      read((uint8_t *)&output.payload, sizeof(CBFloat));
      break;
    case CBType::Float2:
      read((uint8_t *)&output.payload, sizeof(CBFloat2));
      break;
    case CBType::Float3:
      read((uint8_t *)&output.payload, sizeof(CBFloat3));
      break;
    case CBType::Float4:
      read((uint8_t *)&output.payload, sizeof(CBFloat4));
      break;
    case CBType::Color:
      read((uint8_t *)&output.payload, sizeof(CBColor));
      break;
    case CBType::Bytes: {
      auto availBytes = recycle ? output.payload.bytesCapacity : 0;
      read((uint8_t *)&output.payload.bytesSize,
           sizeof(output.payload.bytesSize));

      if (availBytes > 0 && availBytes < output.payload.bytesSize) {
        // not enough space, ideally realloc, but for now just delete
        delete[] output.payload.bytesValue;
        // and re alloc
        output.payload.bytesValue = new uint8_t[output.payload.bytesSize];
      } else if (availBytes == 0) {
        // just alloc
        output.payload.bytesValue = new uint8_t[output.payload.bytesSize];
      } // else got enough space to recycle!

      // record actualSize for further recycling usage
      output.payload.bytesCapacity =
          std::max(availBytes, output.payload.bytesSize);

      read((uint8_t *)output.payload.bytesValue, output.payload.bytesSize);
      break;
    }
    case CBType::Array: {
      read((uint8_t *)&output.innerType, sizeof(output.innerType));
      uint32_t len;
      read((uint8_t *)&len, sizeof(uint32_t));
      chainblocks::arrayResize(output.payload.arrayValue, len);
      read((uint8_t *)&output.payload.arrayValue.elements[0],
           len * sizeof(CBVarPayload));
      break;
    }
    case CBType::Path:
    case CBType::String:
    case CBType::ContextVar: {
      auto availChars = recycle ? output.payload.stringCapacity : 0;
      uint32_t len;
      read((uint8_t *)&len, sizeof(uint32_t));

      if (availChars > 0 && availChars < len) {
        // we need more chars then what we have, realloc
        delete[] output.payload.stringValue;
        output.payload.stringValue = new char[len + 1];
      } else if (availChars == 0) {
        // just alloc
        output.payload.stringValue = new char[len + 1];
      } // else recycling

      // record actualSize
      output.payload.stringCapacity = std::max(availChars, len);

      read((uint8_t *)output.payload.stringValue, len);
      const_cast<char *>(output.payload.stringValue)[len] = 0;
      output.payload.stringLen = len;
      break;
    }
    case CBType::Seq: {
      uint32_t len;
      read((uint8_t *)&len, sizeof(uint32_t));
      // notice we assume all elements up to capacity are memset to 0x0
      // or are valid CBVars we can overwrite
      chainblocks::arrayResize(output.payload.seqValue, len);
      for (uint32_t i = 0; i < len; i++) {
        deserialize(read, output.payload.seqValue.elements[i]);
      }
      break;
    }
    case CBType::Table: {
      CBMap *map = nullptr;

      if (recycle) {
        if (output.payload.tableValue.api && output.payload.tableValue.opaque) {
          map = (CBMap *)output.payload.tableValue.opaque;
          map->clear();
        } else {
          varFree(output);
          output.valueType = nextType;
        }
      }

      if (!map) {
        map = new CBMap();
        output.payload.tableValue.api = &Globals::TableInterface;
        output.payload.tableValue.opaque = map;
      }

      uint64_t len;
      std::string keyBuf;
      read((uint8_t *)&len, sizeof(uint64_t));
      for (uint64_t i = 0; i < len; i++) {
        uint32_t klen;
        read((uint8_t *)&klen, sizeof(uint32_t));
        keyBuf.resize(klen);
        read((uint8_t *)keyBuf.c_str(), klen);
        auto &dst = (*map)[keyBuf];
        deserialize(read, dst);
      }
      break;
    }
    case CBType::Image: {
      size_t currentSize = 0;
      if (recycle) {
        auto bpp = 1;
        if ((output.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
            CBIMAGE_FLAGS_16BITS_INT)
          bpp = 2;
        else if ((output.payload.imageValue.flags &
                  CBIMAGE_FLAGS_32BITS_FLOAT) == CBIMAGE_FLAGS_32BITS_FLOAT)
          bpp = 4;
        currentSize = output.payload.imageValue.channels *
                      output.payload.imageValue.height *
                      output.payload.imageValue.width * bpp;
      }

      read((uint8_t *)&output.payload.imageValue.channels,
           sizeof(output.payload.imageValue.channels));
      read((uint8_t *)&output.payload.imageValue.flags,
           sizeof(output.payload.imageValue.flags));
      read((uint8_t *)&output.payload.imageValue.width,
           sizeof(output.payload.imageValue.width));
      read((uint8_t *)&output.payload.imageValue.height,
           sizeof(output.payload.imageValue.height));

      auto pixsize = 1;
      if ((output.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
          CBIMAGE_FLAGS_16BITS_INT)
        pixsize = 2;
      else if ((output.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
               CBIMAGE_FLAGS_32BITS_FLOAT)
        pixsize = 4;

      size_t size = output.payload.imageValue.channels *
                    output.payload.imageValue.height *
                    output.payload.imageValue.width * pixsize;

      if (currentSize > 0 && currentSize < size) {
        // delete first & alloc
        delete[] output.payload.imageValue.data;
        output.payload.imageValue.data = new uint8_t[size];
      } else if (currentSize == 0) {
        // just alloc
        output.payload.imageValue.data = new uint8_t[size];
      }

      read((uint8_t *)output.payload.imageValue.data, size);
      break;
    }
    case CBType::Block: {
      CBlock *blk;
      uint32_t len;
      read((uint8_t *)&len, sizeof(uint32_t));
      std::vector<char> buf;
      buf.resize(len + 1);
      read((uint8_t *)&buf[0], len);
      buf[len] = 0;
      blk = createBlock(&buf[0]);
      if (!blk) {
        throw CBException("Block not found! name: " + std::string(&buf[0]));
      }
      // validate the hash of the block
      uint32_t crc;
      read((uint8_t *)&crc, sizeof(uint32_t));
      if (blk->hash(blk) != crc) {
        throw CBException("Block hash mismatch, the serialized version is "
                          "probably different: " +
                          std::string(&buf[0]));
      }
      blk->setup(blk);
      auto params = blk->parameters(blk).len + 1;
      while (params--) {
        int idx;
        read((uint8_t *)&idx, sizeof(int));
        if (idx == -1)
          break;
        CBVar tmp{};
        deserialize(read, tmp);
        blk->setParam(blk, idx, &tmp);
        varFree(tmp);
      }
      if (blk->setState) {
        CBVar state{};
        deserialize(read, state);
        blk->setState(blk, &state);
        varFree(state);
      }
      output.payload.blockValue = blk;
      break;
    }
    case CBType::Chain: {
      uint32_t len;
      read((uint8_t *)&len, sizeof(uint32_t));
      std::vector<char> buf;
      buf.resize(len + 1);
      read((uint8_t *)&buf[0], len);
      buf[len] = 0;

      // search if we already have this chain!
      auto cit = chains.find(&buf[0]);
      if (cit != chains.end()) {
        LOG(TRACE) << "Skipping deserializing chain: "
                   << CBChain::sharedFromRef(cit->second)->name;
        output.payload.chainValue = CBChain::addRef(cit->second);
        break;
      }

      auto chain = CBChain::make(&buf[0]);
      output.payload.chainValue = chain->newRef();
      chains.emplace(chain->name, CBChain::addRef(output.payload.chainValue));
      LOG(TRACE) << "Deserializing chain: " << chain->name;
      read((uint8_t *)&chain->looped, 1);
      read((uint8_t *)&chain->unsafe, 1);
      // blocks len
      read((uint8_t *)&len, sizeof(uint32_t));
      // blocks
      for (uint32_t i = 0; i < len; i++) {
        CBVar blockVar{};
        deserialize(read, blockVar);
        assert(blockVar.valueType == Block);
        chain->addBlock(blockVar.payload.blockValue);
        // blow's owner is the chain
      }
      // variables len
      read((uint8_t *)&len, sizeof(uint32_t));
      auto varsLen = len;
      for (uint32_t i = 0; i < varsLen; i++) {
        read((uint8_t *)&len, sizeof(uint32_t));
        buf.resize(len + 1);
        read((uint8_t *)&buf[0], len);
        buf[len] = 0;
        CBVar tmp{};
        deserialize(read, tmp);
        chain->variables[&buf[0]] = tmp;
      }
      break;
    }
    case CBType::Object: {
      int64_t id;
      read((uint8_t *)&id, sizeof(int64_t));
      uint64_t len;
      read((uint8_t *)&len, sizeof(uint64_t));
      if (len > 0) {
        auto it = Globals::ObjectTypesRegister.find(id);
        if (it != Globals::ObjectTypesRegister.end()) {
          auto &info = it->second;
          auto size = size_t(len);
          std::vector<uint8_t> data;
          data.resize(size);
          read((uint8_t *)&data[0], size);
          output.payload.objectValue = info.deserialize(&data[0], size);
          int32_t vendorId = (int32_t)((id & 0xFFFFFFFF00000000) >> 32);
          int32_t typeId = (int32_t)(id & 0x00000000FFFFFFFF);
          output.payload.objectVendorId = vendorId;
          output.payload.objectTypeId = typeId;
          output.flags |= CBVAR_FLAGS_USES_OBJINFO;
          output.objectInfo = &info;
          // up ref count also, not included in deserialize op!
          if (info.reference) {
            info.reference(output.payload.objectValue);
          }
        } else {
          throw chainblocks::CBException(
              "Failed to find object type in registry.");
        }
      }
      break;
    }
    }
  }

  template <class BinaryWriter>
  size_t serialize(const CBVar &input, BinaryWriter &write) {
    size_t total = 0;
    write((const uint8_t *)&input.valueType, sizeof(input.valueType));
    total += sizeof(input.valueType);
    switch (input.valueType) {
    case CBType::None:
    case CBType::EndOfBlittableTypes:
    case CBType::Any:
      break;
    case CBType::Enum:
      write((const uint8_t *)&input.payload, sizeof(int32_t) * 3);
      total += sizeof(int32_t) * 3;
      break;
    case CBType::Bool:
      write((const uint8_t *)&input.payload, sizeof(CBBool));
      total += sizeof(CBBool);
      break;
    case CBType::Int:
      write((const uint8_t *)&input.payload, sizeof(CBInt));
      total += sizeof(CBInt);
      break;
    case CBType::Int2:
      write((const uint8_t *)&input.payload, sizeof(CBInt2));
      total += sizeof(CBInt2);
      break;
    case CBType::Int3:
      write((const uint8_t *)&input.payload, sizeof(CBInt3));
      total += sizeof(CBInt3);
      break;
    case CBType::Int4:
      write((const uint8_t *)&input.payload, sizeof(CBInt4));
      total += sizeof(CBInt4);
      break;
    case CBType::Int8:
      write((const uint8_t *)&input.payload, sizeof(CBInt8));
      total += sizeof(CBInt8);
      break;
    case CBType::Int16:
      write((const uint8_t *)&input.payload, sizeof(CBInt16));
      total += sizeof(CBInt16);
      break;
    case CBType::Float:
      write((const uint8_t *)&input.payload, sizeof(CBFloat));
      total += sizeof(CBFloat);
      break;
    case CBType::Float2:
      write((const uint8_t *)&input.payload, sizeof(CBFloat2));
      total += sizeof(CBFloat2);
      break;
    case CBType::Float3:
      write((const uint8_t *)&input.payload, sizeof(CBFloat3));
      total += sizeof(CBFloat3);
      break;
    case CBType::Float4:
      write((const uint8_t *)&input.payload, sizeof(CBFloat4));
      total += sizeof(CBFloat4);
      break;
    case CBType::Color:
      write((const uint8_t *)&input.payload, sizeof(CBColor));
      total += sizeof(CBColor);
      break;
    case CBType::Bytes:
      write((const uint8_t *)&input.payload.bytesSize,
            sizeof(input.payload.bytesSize));
      total += sizeof(input.payload.bytesSize);
      write((const uint8_t *)input.payload.bytesValue, input.payload.bytesSize);
      total += input.payload.bytesSize;
      break;
    case CBType::Array: {
      write((const uint8_t *)&input.innerType, sizeof(input.innerType));
      total += sizeof(input.innerType);
      write((const uint8_t *)&input.payload.arrayValue.len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      auto size = input.payload.arrayValue.len * sizeof(CBVarPayload);
      write((const uint8_t *)&input.payload.arrayValue.elements[0], size);
      total += size;
    } break;
    case CBType::Path:
    case CBType::String:
    case CBType::ContextVar: {
      uint32_t len = input.payload.stringLen > 0
                         ? input.payload.stringLen
                         : uint32_t(strlen(input.payload.stringValue));
      write((const uint8_t *)&len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      write((const uint8_t *)input.payload.stringValue, len);
      total += len;
      break;
    }
    case CBType::Seq: {
      uint32_t len = input.payload.seqValue.len;
      write((const uint8_t *)&len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      for (uint32_t i = 0; i < len; i++) {
        total += serialize(input.payload.seqValue.elements[i], write);
      }
      break;
    }
    case CBType::Table: {
      if (input.payload.tableValue.api && input.payload.tableValue.opaque) {
        auto &t = input.payload.tableValue;
        uint64_t len = (uint64_t)t.api->tableSize(t);
        write((const uint8_t *)&len, sizeof(uint64_t));
        total += sizeof(uint64_t);
        struct iterdata {
          BinaryWriter *write;
          size_t *total;
          Serialization *s;
        } data;
        data.write = &write;
        data.total = &total;
        data.s = this;
        t.api->tableForEach(
            t,
            [](const char *key, CBVar *value, void *_data) {
              auto data = (iterdata *)_data;
              uint32_t klen = strlen(key);
              (*data->write)((const uint8_t *)&klen, sizeof(uint32_t));
              *data->total += sizeof(uint32_t);
              (*data->write)((const uint8_t *)key, klen);
              *data->total += klen;
              *data->total += data->s->serialize(*value, *data->write);
              return true;
            },
            &data);
      } else {
        uint64_t none = 0;
        write((const uint8_t *)&none, sizeof(uint64_t));
        total += sizeof(uint64_t);
      }
      break;
    }
    case CBType::Image: {
      auto pixsize = 1;
      if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
          CBIMAGE_FLAGS_16BITS_INT)
        pixsize = 2;
      else if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
               CBIMAGE_FLAGS_32BITS_FLOAT)
        pixsize = 4;
      write((const uint8_t *)&input.payload.imageValue.channels,
            sizeof(input.payload.imageValue.channels));
      total += sizeof(input.payload.imageValue.channels);
      write((const uint8_t *)&input.payload.imageValue.flags,
            sizeof(input.payload.imageValue.flags));
      total += sizeof(input.payload.imageValue.flags);
      write((const uint8_t *)&input.payload.imageValue.width,
            sizeof(input.payload.imageValue.width));
      total += sizeof(input.payload.imageValue.width);
      write((const uint8_t *)&input.payload.imageValue.height,
            sizeof(input.payload.imageValue.height));
      total += sizeof(input.payload.imageValue.height);
      auto size = input.payload.imageValue.channels *
                  input.payload.imageValue.height *
                  input.payload.imageValue.width * pixsize;
      write((const uint8_t *)input.payload.imageValue.data, size);
      total += size;
      break;
    }
    case CBType::Block: {
      auto blk = input.payload.blockValue;
      // name
      auto name = blk->name(blk);
      uint32_t len = uint32_t(strlen(name));
      write((const uint8_t *)&len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      write((const uint8_t *)name, len);
      total += len;
      // serialize the hash of the block as well
      auto crc = blk->hash(blk);
      write((const uint8_t *)&crc, sizeof(uint32_t));
      total += sizeof(uint32_t);
      // params
      // well, this is bad and should be fixed somehow at some point
      // we are creating a block just to compare to figure default values
      auto model =
          defaultBlocks
              .emplace(name, std::shared_ptr<CBlock>(
                                 createBlock(name),
                                 [](CBlock *block) { block->destroy(block); }))
              .first->second.get();
      auto params = blk->parameters(blk);
      for (uint32_t i = 0; i < params.len; i++) {
        auto idx = int(i);
        auto dval = model->getParam(model, idx);
        auto pval = blk->getParam(blk, idx);
        if (pval != dval) {
          write((const uint8_t *)&idx, sizeof(int));
          total += serialize(pval, write) + sizeof(int);
        }
      }
      int idx = -1; // end of params
      write((const uint8_t *)&idx, sizeof(int));
      total += sizeof(int);
      // optional state
      if (blk->getState) {
        auto state = blk->getState(blk);
        total += serialize(state, write);
      }
      break;
    }
    case CBType::Chain: {
      auto sc = CBChain::sharedFromRef(input.payload.chainValue);
      auto chain = sc.get();

      { // Name
        uint32_t len = uint32_t(chain->name.size());
        write((const uint8_t *)&len, sizeof(uint32_t));
        total += sizeof(uint32_t);
        write((const uint8_t *)chain->name.c_str(), len);
        total += len;
      }

      // stop here if we had it already
      if (chains.count(chain->name) > 0) {
        LOG(TRACE) << "Skipping serializing chain: " << chain->name;
        break;
      }

      LOG(TRACE) << "Serializing chain: " << chain->name;
      chains.emplace(chain->name, CBChain::addRef(input.payload.chainValue));

      { // Looped & Unsafe
        write((const uint8_t *)&chain->looped, 1);
        total += 1;
        write((const uint8_t *)&chain->unsafe, 1);
        total += 1;
      }
      { // Blocks len
        uint32_t len = uint32_t(chain->blocks.size());
        write((const uint8_t *)&len, sizeof(uint32_t));
        total += sizeof(uint32_t);
      }
      // Blocks
      for (auto block : chain->blocks) {
        CBVar blockVar{};
        blockVar.valueType = CBType::Block;
        blockVar.payload.blockValue = block;
        total += serialize(blockVar, write);
      }
      { // Variables len
        uint32_t len = uint32_t(chain->variables.size());
        write((const uint8_t *)&len, sizeof(uint32_t));
        total += sizeof(uint32_t);
      }
      // Variables
      for (auto &var : chain->variables) {
        uint32_t len = uint32_t(var.first.size());
        write((const uint8_t *)&len, sizeof(uint32_t));
        total += sizeof(uint32_t);
        write((const uint8_t *)var.first.c_str(), len);
        total += len;
        // Serialization discards anything cept payload
        // That is what we want anyway!
        total += serialize(var.second, write);
      }
      break;
    }
    case CBType::Object: {
      int64_t id = (int64_t)input.payload.objectVendorId << 32 |
                   input.payload.objectTypeId;
      write((const uint8_t *)&id, sizeof(int64_t));
      total += sizeof(int64_t);
      if ((input.flags & CBVAR_FLAGS_USES_OBJINFO) ==
              CBVAR_FLAGS_USES_OBJINFO &&
          input.objectInfo && input.objectInfo->serialize) {
        size_t len = 0;
        uint8_t *data = nullptr;
        CBPointer handle = nullptr;
        if (!input.objectInfo->serialize(input.payload.objectValue, &data, &len,
                                         &handle)) {
          throw chainblocks::CBException(
              "Failed to serialize custom object variable!");
        }
        uint64_t ulen = uint64_t(len);
        write((const uint8_t *)&ulen, sizeof(uint64_t));
        total += sizeof(uint64_t);
        write((const uint8_t *)&data[0], len);
        total += len;
        input.objectInfo->free(handle);
      } else {
        uint64_t empty = 0;
        write((const uint8_t *)&empty, sizeof(uint64_t));
        total += sizeof(uint64_t);
      }
      break;
    }
    }
    return total;
  }
};

template <typename T> struct ChainDoppelgangerPool {
  ChainDoppelgangerPool(CBChainRef master) {
    auto vchain = Var(master);
    std::stringstream stream;
    Writer w(stream);
    Serialization serializer;
    serializer.serialize(vchain, w);
    _chainStr = stream.str();
  }

  template <class Composer> std::shared_ptr<T> acquire(Composer &composer) {
    if (_avail.size() == 0) {
      Serialization serializer;
      std::stringstream stream(_chainStr);
      Reader r(stream);
      CBVar vchain{};
      serializer.deserialize(r, vchain);
      auto chain = CBChain::sharedFromRef(vchain.payload.chainValue);
      auto fresh = _pool.emplace_back(std::make_shared<T>());
      fresh->chain = chain;
      composer.compose(chain.get());
      fresh->chain->name =
          fresh->chain->name + "-" + std::to_string(_pool.size());
      return fresh;
    } else {
      auto res = _avail.back();
      _avail.pop_back();
      return res;
    }
  }

  void release(std::shared_ptr<T> chain) { _avail.emplace_back(chain); }

private:
  struct Writer {
    std::stringstream &stream;
    Writer(std::stringstream &stream) : stream(stream) {}
    void operator()(const uint8_t *buf, size_t size) {
      stream.write((const char *)buf, size);
    }
  };

  struct Reader {
    std::stringstream &stream;
    Reader(std::stringstream &stream) : stream(stream) {}
    void operator()(uint8_t *buf, size_t size) {
      stream.read((char *)buf, size);
    }
  };

  // keep our pool in a deque in order to keep them alive
  // so users don't have to worry about lifetime
  // just release when possible
  std::deque<std::shared_ptr<T>> _pool;
  std::vector<std::shared_ptr<T>> _avail;
  std::string _chainStr;
};
} // namespace chainblocks

#endif

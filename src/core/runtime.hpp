/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_RUNTIME
#define SH_CORE_RUNTIME

// must go first
#if _WIN32
#include <winsock2.h>
#endif

#include <boost/asio/dispatch.hpp>
#include <boost/asio/thread_pool.hpp>

#include <string.h> // memset

#include "shards_macros.hpp"
#include "foundation.hpp"

#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using SHClock = std::chrono::high_resolution_clock;
using SHTime = decltype(SHClock::now());
using SHDuration = std::chrono::duration<double>;
using SHTimeDiff = decltype(SHClock::now() - SHDuration(0.0));

// For sleep
#if _WIN32
#include <Windows.h>
#else
#include <time.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/val.h>
#endif

#define XXH_INLINE_ALL
#include <xxhash.h>

#ifndef CUSTOM_XXH3_kSecret
// Applications embedding shards can override this and should.
// TODO add our secret
#define CUSTOM_XXH3_kSecret XXH3_kSecret
#endif

#define SH_SUSPEND(_ctx_, _secs_)                                  \
  const auto _suspend_state = shards::suspend(_ctx_, _secs_); \
  if (_suspend_state != SHWireState::Continue)                    \
  return shards::Var::Empty

#define SH_STOP() std::rethrow_exception(shards::GetGlobals().StopWireEx);
#define SH_RESTART() std::rethrow_exception(shards::GetGlobals().RestartWireEx);

struct SHContext {
  SHContext(
#ifndef __EMSCRIPTEN__
      SHCoro &&sink,
#else
      SHCoro *coro,
#endif
      const SHWire *starter, SHFlow *flow)
      : main(starter), flow(flow),
#ifndef __EMSCRIPTEN__
        continuation(std::move(sink))
#else
        continuation(coro)
#endif
  {
    wireStack.push_back(const_cast<SHWire *>(starter));
  }

  const SHWire *main;
  SHFlow *flow;
  std::vector<SHWire *> wireStack;
  bool onCleanup{false};
  bool onLastResume{false};

// Used within the coro& stack! (suspend, etc)
#ifndef __EMSCRIPTEN__
  SHCoro &&continuation;
#else
  SHCoro *continuation{nullptr};
#endif
  SHDuration next{};
#ifdef SH_USE_TSAN
  void *tsan_handle = nullptr;
#endif

  SHWire *currentWire() const { return wireStack.back(); }

  constexpr void stopFlow(const SHVar &lastValue) {
    state = SHWireState::Stop;
    flowStorage = lastValue;
  }

  constexpr void restartFlow(const SHVar &lastValue) {
    state = SHWireState::Restart;
    flowStorage = lastValue;
  }

  constexpr void returnFlow(const SHVar &lastValue) {
    state = SHWireState::Return;
    flowStorage = lastValue;
  }

  void cancelFlow(std::string_view message) {
    state = SHWireState::Stop;
    errorMessage = message;
    hasError = true;
  }

  void resetCancelFlow() {
    state = SHWireState::Continue;
    hasError = false;
  }

  constexpr void rebaseFlow() { state = SHWireState::Rebase; }

  constexpr void continueFlow() { state = SHWireState::Continue; }

  constexpr bool shouldContinue() const { return state == SHWireState::Continue; }

  constexpr bool shouldReturn() const { return state == SHWireState::Return; }

  constexpr bool shouldStop() const { return state == SHWireState::Stop; }

  constexpr bool failed() const { return hasError; }

  constexpr const std::string &getErrorMessage() { return errorMessage; }

  constexpr SHWireState getState() const { return state; }

  constexpr SHVar getFlowStorage() const { return flowStorage; }

  void registerNextFrameCallback(const Shard *shard) const { nextFrameShards.push_back(shard); }

  const std::vector<const Shard *> &nextFrameCallbacks() const { return nextFrameShards; }

private:
  SHWireState state = SHWireState::Continue;
  // Used when flow is stopped/restart/return
  // to store the previous result
  SHVar flowStorage{};
  bool hasError{false};
  std::string errorMessage;
  mutable std::vector<const Shard *> nextFrameShards;
};

namespace shards {
[[nodiscard]] SHComposeResult composeWire(const std::vector<Shard *> &wire, SHValidationCallback callback, void *userData,
                                           SHInstanceData data);
[[nodiscard]] SHComposeResult composeWire(const Shards wire, SHValidationCallback callback, void *userData,
                                           SHInstanceData data);
[[nodiscard]] SHComposeResult composeWire(const SHSeq wire, SHValidationCallback callback, void *userData, SHInstanceData data);
[[nodiscard]] SHComposeResult composeWire(const SHWire *wire, SHValidationCallback callback, void *userData,
                                           SHInstanceData data);

bool validateSetParam(Shard *shard, int index, const SHVar &value, SHValidationCallback callback, void *userData);
} // namespace shards

#include "shards/core.hpp"
#include "shards/math.hpp"

namespace shards {

void installSignalHandlers();

FLATTEN ALWAYS_INLINE inline SHVar activateShard(Shard *blk, SHContext *context, const SHVar &input) {
  switch (blk->inlineShardId) {
  case NoopShard:
    return input;
  case CoreConst: {
    auto shard = reinterpret_cast<shards::ShardWrapper<Const> *>(blk);
    return shard->shard._value;
  }
  case CoreIs: {
    auto shard = reinterpret_cast<shards::IsRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreIsNot: {
    auto shard = reinterpret_cast<shards::IsNotRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreAnd: {
    auto shard = reinterpret_cast<shards::AndRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreOr: {
    auto shard = reinterpret_cast<shards::OrRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreNot: {
    auto shard = reinterpret_cast<shards::NotRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreIsMore: {
    auto shard = reinterpret_cast<shards::IsMoreRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreIsLess: {
    auto shard = reinterpret_cast<shards::IsLessRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreIsMoreEqual: {
    auto shard = reinterpret_cast<shards::IsMoreEqualRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreIsLessEqual: {
    auto shard = reinterpret_cast<shards::IsLessEqualRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreSleep: {
    auto shard = reinterpret_cast<shards::ShardWrapper<Pause> *>(blk);
    return shard->shard.activate(context, input);
  }
  case CoreInput: {
    auto shard = reinterpret_cast<shards::ShardWrapper<Input> *>(blk);
    return shard->shard.activate(context, input);
  }
  case CorePush: {
    auto shard = reinterpret_cast<shards::PushRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreForRange: {
    auto shard = reinterpret_cast<shards::ShardWrapper<ForRangeShard> *>(blk);
    return shard->shard.activate(context, input);
  }
  case CoreRepeat: {
    auto shard = reinterpret_cast<shards::RepeatRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreOnce: {
    auto shard = reinterpret_cast<shards::ShardWrapper<Once> *>(blk);
    return shard->shard.activate(context, input);
  }
  case CoreGet: {
    auto shard = reinterpret_cast<shards::GetRuntime *>(blk);
    return *shard->core._cell;
  }
  case CoreSet: {
    auto shard = reinterpret_cast<shards::SetRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreRefTable: {
    auto shard = reinterpret_cast<shards::RefRuntime *>(blk);
    return shard->core.activateTable(context, input);
  }
  case CoreRefRegular: {
    auto shard = reinterpret_cast<shards::RefRuntime *>(blk);
    return shard->core.activateRegular(context, input);
  }
  case CoreUpdate: {
    auto shard = reinterpret_cast<shards::UpdateRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case CoreSwap: {
    auto shard = reinterpret_cast<shards::SwapRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathAdd: {
    auto shard = reinterpret_cast<shards::Math::AddRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathSubtract: {
    auto shard = reinterpret_cast<shards::Math::SubtractRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathMultiply: {
    auto shard = reinterpret_cast<shards::Math::MultiplyRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathDivide: {
    auto shard = reinterpret_cast<shards::Math::DivideRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathXor: {
    auto shard = reinterpret_cast<shards::Math::XorRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathAnd: {
    auto shard = reinterpret_cast<shards::Math::AndRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathOr: {
    auto shard = reinterpret_cast<shards::Math::OrRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathMod: {
    auto shard = reinterpret_cast<shards::Math::ModRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathLShift: {
    auto shard = reinterpret_cast<shards::Math::LShiftRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathRShift: {
    auto shard = reinterpret_cast<shards::Math::RShiftRuntime *>(blk);
    return shard->core.activate(context, input);
  }
  case MathAbs: {
    auto shard = reinterpret_cast<shards::Math::AbsRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathFastSqrt: {
    auto shard = reinterpret_cast<shards::Math::FastSqrtRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathFastInvSqrt: {
    auto shard = reinterpret_cast<shards::Math::FastInvSqrtRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
#if 0
  case MathExp: {
    auto shard = reinterpret_cast<shards::Math::ExpRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathExp2: {
    auto shard = reinterpret_cast<shards::Math::Exp2Runtime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathExpm1: {
    auto shard = reinterpret_cast<shards::Math::Expm1Runtime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathLog: {
    auto shard = reinterpret_cast<shards::Math::LogRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathLog10: {
    auto shard = reinterpret_cast<shards::Math::Log10Runtime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathLog2: {
    auto shard = reinterpret_cast<shards::Math::Log2Runtime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathLog1p: {
    auto shard = reinterpret_cast<shards::Math::Log1pRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathSqrt: {
    auto shard = reinterpret_cast<shards::Math::SqrtRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathCbrt: {
    auto shard = reinterpret_cast<shards::Math::CbrtRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathSin: {
    auto shard = reinterpret_cast<shards::Math::SinRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathCos: {
    auto shard = reinterpret_cast<shards::Math::CosRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathTan: {
    auto shard = reinterpret_cast<shards::Math::TanRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathAsin: {
    auto shard = reinterpret_cast<shards::Math::AsinRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathAcos: {
    auto shard = reinterpret_cast<shards::Math::AcosRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathAtan: {
    auto shard = reinterpret_cast<shards::Math::AtanRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathSinh: {
    auto shard = reinterpret_cast<shards::Math::SinhRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathCosh: {
    auto shard = reinterpret_cast<shards::Math::CoshRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathTanh: {
    auto shard = reinterpret_cast<shards::Math::TanhRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathAsinh: {
    auto shard = reinterpret_cast<shards::Math::AsinhRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathAcosh: {
    auto shard = reinterpret_cast<shards::Math::AcoshRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathAtanh: {
    auto shard = reinterpret_cast<shards::Math::AtanhRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathErf: {
    auto shard = reinterpret_cast<shards::Math::ErfRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathErfc: {
    auto shard = reinterpret_cast<shards::Math::ErfcRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathTGamma: {
    auto shard = reinterpret_cast<shards::Math::TGammaRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathLGamma: {
    auto shard = reinterpret_cast<shards::Math::LGammaRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
#endif
  case MathCeil: {
    auto shard = reinterpret_cast<shards::Math::CeilRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathFloor: {
    auto shard = reinterpret_cast<shards::Math::FloorRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathTrunc: {
    auto shard = reinterpret_cast<shards::Math::TruncRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  case MathRound: {
    auto shard = reinterpret_cast<shards::Math::RoundRuntime *>(blk);
    return shard->core.activateSingle(context, input);
  }
  default: {
    // NotInline
    return blk->activate(blk, context, &input);
  }
  }
}

SHRunWireOutput runWire(SHWire *wire, SHContext *context, const SHVar &wireInput);

inline SHRunWireOutput runSubWire(SHWire *wire, SHContext *context, const SHVar &input) {
  // push to wire stack
  context->wireStack.push_back(wire);
  DEFER({ context->wireStack.pop_back(); });
  auto runRes = shards::runWire(wire, context, input);
  return runRes;
}

#ifndef __EMSCRIPTEN__
boost::context::continuation run(SHWire *wire, SHFlow *flow, boost::context::continuation &&sink);
#else
void run(SHWire *wire, SHFlow *flow, SHCoro *coro);
#endif

inline void prepare(SHWire *wire, SHFlow *flow) {
  if (wire->coro)
    return;

#ifdef SH_USE_TSAN
  auto curr = __tsan_get_current_fiber();
  if (wire->tsan_coro)
    __tsan_destroy_fiber(wire->tsan_coro);
  wire->tsan_coro = __tsan_create_fiber(0);
  __tsan_switch_to_fiber(wire->tsan_coro, 0);
#endif

#ifndef __EMSCRIPTEN__
  if (!wire->stackMem) {
    wire->stackMem = new (std::align_val_t{16}) uint8_t[wire->stackSize];
  }
  wire->coro =
      boost::context::callcc(std::allocator_arg, SHStackAllocator{wire->stackSize, wire->stackMem},
                             [wire, flow](boost::context::continuation &&sink) { return run(wire, flow, std::move(sink)); });
#else
  wire->coro.emplace(wire->stackSize);
  wire->coro->init([=]() { run(wire, flow, &(*wire->coro)); });
  wire->coro->resume();
#endif

#ifdef SH_USE_TSAN
  __tsan_switch_to_fiber(curr, 0);
#endif
}

inline void start(SHWire *wire, SHVar input = {}) {
  if (wire->state != SHWire::State::Prepared) {
    SHLOG_ERROR("Attempted to start a wire not ready for running!");
    return;
  }

  if (!wire->coro || !(*wire->coro))
    return; // check if not null and bool operator also to see if alive!

  shards::cloneVar(wire->rootTickInput, input);
  wire->state = SHWire::State::Starting;

  for (auto &call : wire->onStart) {
    call();
  }
}

inline bool stop(SHWire *wire, SHVar *result = nullptr) {
  if (wire->state == SHWire::State::Stopped) {
    // Clone the results if we need them
    if (result)
      cloneVar(*result, wire->finishedOutput);

    return true;
  }

  SHLOG_TRACE("stopping wire: {}", wire->name);

  if (wire->coro) {
    // Run until exit if alive, need to propagate to all suspended shards!
    if ((*wire->coro) && wire->state > SHWire::State::Stopped && wire->state < SHWire::State::Failed) {
      // set abortion flag, we always have a context in this case
      wire->context->stopFlow(shards::Var::Empty);
      wire->context->onLastResume = true;

      // BIG Warning: wire->context existed in the coro stack!!!
      // after this resume wire->context is trash!
#ifdef SH_USE_TSAN
      auto curr = __tsan_get_current_fiber();
      __tsan_switch_to_fiber(wire->tsan_coro, 0);
#endif

      wire->coro->resume();

#ifdef SH_USE_TSAN
      __tsan_switch_to_fiber(curr, 0);
#endif
    }

    // delete also the coro ptr
    wire->coro.reset();
  } else {
    // if we had a coro this will run inside it!
    wire->cleanup(true);
  }

  // return true if we ended, as in we did our job
  auto res = wire->state == SHWire::State::Ended;

  wire->state = SHWire::State::Stopped;
  destroyVar(wire->rootTickInput);

  // Clone the results if we need them
  if (result)
    cloneVar(*result, wire->finishedOutput);

  for (auto &call : wire->onStop) {
    call();
  }

  return res;
}

inline bool isRunning(SHWire *wire) {
  const auto state = wire->state.load(); // atomic
  return state >= SHWire::State::Starting && state <= SHWire::State::IterationEnded;
}

inline bool tick(SHWire *wire, SHDuration now, SHVar rootInput = {}) {
  if (!wire->context || !wire->coro || !(*wire->coro) || !(isRunning(wire)))
    return false; // check if not null and bool operator also to see if alive!

  if (now >= wire->context->next) {
    if (rootInput != shards::Var::Empty) {
      cloneVar(wire->rootTickInput, rootInput);
    }
#ifdef SH_USE_TSAN
    auto curr = __tsan_get_current_fiber();
    __tsan_switch_to_fiber(wire->tsan_coro, 0);
#endif

#ifndef __EMSCRIPTEN__
    *wire->coro = wire->coro->resume();
#else
    wire->coro->resume();
#endif

#ifdef SH_USE_TSAN
    __tsan_switch_to_fiber(curr, 0);
#endif
  }
  return true;
}

inline bool hasEnded(SHWire *wire) { return wire->state > SHWire::State::IterationEnded; }

inline bool isCanceled(SHContext *context) { return context->shouldStop(); }

inline void sleep(double seconds = -1.0, bool runCallbacks = true) {
  // negative = no sleep, just run callbacks

  // Run callbacks first if needed
  // Take note of how long it took and subtract from sleep time! if some time is
  // left sleep
  if (runCallbacks) {
    SHDuration sleepTime(seconds);
    auto pre = SHClock::now();
    for (auto &shinfo : GetGlobals().RunLoopHooks) {
      if (shinfo.second) {
        shinfo.second();
      }
    }
    auto post = SHClock::now();

    SHDuration shsTime = post - pre;
    SHDuration realSleepTime = sleepTime - shsTime;
    if (seconds != -1.0 && realSleepTime.count() > 0.0) {
      // Sleep actual time minus stuff we did in shs
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
  virtual void registerShard(const char *fullName, SHShardConstructor constructor) = 0;
  virtual void registerObjectType(int32_t vendorId, int32_t typeId, SHObjectInfo info) = 0;
  virtual void registerEnumType(int32_t vendorId, int32_t typeId, SHEnumInfo info) = 0;
};
}; // namespace shards

struct SHMesh : public std::enable_shared_from_this<SHMesh> {
  static std::shared_ptr<SHMesh> make() { return std::shared_ptr<SHMesh>(new SHMesh()); }

  static std::shared_ptr<SHMesh> *makePtr() { return new std::shared_ptr<SHMesh>(new SHMesh()); }

  ~SHMesh() { terminate(); }

  struct EmptyObserver {
    void before_compose(SHWire *wire) {}
    void before_tick(SHWire *wire) {}
    void before_stop(SHWire *wire) {}
    void before_prepare(SHWire *wire) {}
    void before_start(SHWire *wire) {}
  };

  template <class Observer>
  void schedule(Observer observer, const std::shared_ptr<SHWire> &wire, SHVar input = shards::Var::Empty,
                bool compose = true) {
    if (wire->warmedUp) {
      SHLOG_ERROR("Attempted to schedule a wire multiple times, wire: {}", wire->name);
      throw shards::SHException("Multiple wire schedule");
    }

    // this is to avoid recursion during compose
    visitedWires.clear();

    wire->mesh = shared_from_this();
    wire->isRoot = true;
    // remove when done here
    DEFER(wire->isRoot = false);

    observer.before_compose(wire.get());
    if (compose) {
      // compose the wire
      SHInstanceData data = instanceData;
      data.wire = wire.get();
      data.inputType = shards::deriveTypeInfo(input, data);
      auto validation = shards::composeWire(
          wire.get(),
          [](const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
            auto blk = const_cast<Shard *>(errorShard);
            if (!nonfatalWarning) {
              throw shards::ComposeError(std::string(errorTxt) + ", input shard: " + std::string(blk->name(blk)));
            } else {
              SHLOG_INFO("Validation warning: {} input shard: {}", errorTxt, blk->name(blk));
            }
          },
          this, data);
      shards::arrayFree(validation.exposedInfo);
      shards::arrayFree(validation.requiredInfo);
      shards::freeDerivedInfo(data.inputType);
    }

    observer.before_prepare(wire.get());
    // create a flow as well
    shards::prepare(wire.get(), _flows.emplace_back(new SHFlow{wire.get()}).get());
    observer.before_start(wire.get());
    shards::start(wire.get(), input);

    scheduled.insert(wire);
  }

  void schedule(const std::shared_ptr<SHWire> &wire, SHVar input = shards::Var::Empty, bool compose = true) {
    EmptyObserver obs;
    schedule(obs, wire, input, compose);
  }

  template <class Observer> bool tick(Observer observer, SHVar input = shards::Var::Empty) {
    auto noErrors = true;
    _errors.clear();
    if (shards::GetGlobals().SigIntTerm > 0) {
      terminate();
    } else {
      SHDuration now = SHClock::now().time_since_epoch();
      for (auto it = _flows.begin(); it != _flows.end();) {
        auto &flow = *it;
        observer.before_tick(flow->wire);
        shards::tick(flow->wire, now, input);
        if (unlikely(!shards::isRunning(flow->wire))) {
          if (flow->wire->finishedError.size() > 0) {
            _errors.emplace_back(flow->wire->finishedError);
          }
          observer.before_stop(flow->wire);
          if (!shards::stop(flow->wire)) {
            noErrors = false;
          }
          flow->wire->mesh.reset();
          it = _flows.erase(it);
        } else {
          ++it;
        }
      }
    }
    return noErrors;
  }

  bool tick(SHVar input = shards::Var::Empty) {
    EmptyObserver obs;
    return tick(obs, input);
  }

  void terminate() {
    for (auto wire : scheduled) {
      shards::stop(wire.get());
      wire->mesh.reset();
    }

    _flows.clear();

    // release all wires
    scheduled.clear();

    // find dangling variables and notice
    for (auto var : variables) {
      if (var.second.refcount > 0) {
        SHLOG_ERROR("Found a dangling global variable: {}", var.first);
      }
    }
    variables.clear();

    // whichever shard uses refs must clean them
    refs.clear();
  }

  void remove(const std::shared_ptr<SHWire> &wire) {
    shards::stop(wire.get());
    _flows.remove_if([wire](auto &flow) { return flow->wire == wire.get(); });
    wire->mesh.reset();
    visitedWires.erase(wire.get());
    scheduled.erase(wire);
  }

  bool empty() { return _flows.empty(); }

  const std::vector<std::string> &errors() { return _errors; }

  std::unordered_map<std::string, SHVar, std::hash<std::string>, std::equal_to<std::string>,
                     boost::alignment::aligned_allocator<std::pair<const std::string, SHVar>, 16>>
      variables;

  std::unordered_map<std::string, SHVar *> refs;

  std::unordered_map<SHWire *, SHTypeInfo> visitedWires;

  std::unordered_set<std::shared_ptr<SHWire>> scheduled;

  SHInstanceData instanceData{};

private:
  std::list<std::shared_ptr<SHFlow>> _flows;
  std::vector<std::string> _errors;
  SHMesh() = default;
};

namespace shards {
struct Serialization {
  static void varFree(SHVar &output);

  std::unordered_map<std::string, SHWireRef> wires;
  std::unordered_map<std::string, std::shared_ptr<Shard>> defaultShards;

  void reset() {
    for (auto &ref : wires) {
      SHWire::deleteRef(ref.second);
    }
    wires.clear();
    defaultShards.clear();
  }

  ~Serialization() { reset(); }

  template <class BinaryReader> void deserialize(BinaryReader &read, SHVar &output) {
    // we try to recycle memory so pass a empty None as first timer!
    SHType nextType;
    read((uint8_t *)&nextType, sizeof(output.valueType));

    // stop trying to recycle, types differ
    auto recycle = true;
    if (output.valueType != nextType) {
      varFree(output);
      recycle = false;
    }

    output.valueType = nextType;

    switch (output.valueType) {
    case SHType::None:
    case SHType::EndOfBlittableTypes:
    case SHType::Any:
      break;
    case SHType::Enum:
      read((uint8_t *)&output.payload, sizeof(int32_t) * 3);
      break;
    case SHType::Bool:
      read((uint8_t *)&output.payload, sizeof(SHBool));
      break;
    case SHType::Int:
      read((uint8_t *)&output.payload, sizeof(SHInt));
      break;
    case SHType::Int2:
      read((uint8_t *)&output.payload, sizeof(SHInt2));
      break;
    case SHType::Int3:
      read((uint8_t *)&output.payload, sizeof(SHInt3));
      break;
    case SHType::Int4:
      read((uint8_t *)&output.payload, sizeof(SHInt4));
      break;
    case SHType::Int8:
      read((uint8_t *)&output.payload, sizeof(SHInt8));
      break;
    case SHType::Int16:
      read((uint8_t *)&output.payload, sizeof(SHInt16));
      break;
    case SHType::Float:
      read((uint8_t *)&output.payload, sizeof(SHFloat));
      break;
    case SHType::Float2:
      read((uint8_t *)&output.payload, sizeof(SHFloat2));
      break;
    case SHType::Float3:
      read((uint8_t *)&output.payload, sizeof(SHFloat3));
      break;
    case SHType::Float4:
      read((uint8_t *)&output.payload, sizeof(SHFloat4));
      break;
    case SHType::Color:
      read((uint8_t *)&output.payload, sizeof(SHColor));
      break;
    case SHType::Bytes: {
      auto availBytes = recycle ? output.payload.bytesCapacity : 0;
      read((uint8_t *)&output.payload.bytesSize, sizeof(output.payload.bytesSize));

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
      output.payload.bytesCapacity = std::max(availBytes, output.payload.bytesSize);

      read((uint8_t *)output.payload.bytesValue, output.payload.bytesSize);
      break;
    }
    case SHType::Array: {
      read((uint8_t *)&output.innerType, sizeof(output.innerType));
      uint32_t len;
      read((uint8_t *)&len, sizeof(uint32_t));
      shards::arrayResize(output.payload.arrayValue, len);
      read((uint8_t *)&output.payload.arrayValue.elements[0], len * sizeof(SHVarPayload));
      break;
    }
    case SHType::Path:
    case SHType::String:
    case SHType::ContextVar: {
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
    case SHType::Seq: {
      uint32_t len;
      read((uint8_t *)&len, sizeof(uint32_t));
      // notice we assume all elements up to capacity are memset to 0x0
      // or are valid SHVars we can overwrite
      shards::arrayResize(output.payload.seqValue, len);
      for (uint32_t i = 0; i < len; i++) {
        deserialize(read, output.payload.seqValue.elements[i]);
      }
      break;
    }
    case SHType::Table: {
      SHMap *map = nullptr;

      if (recycle) {
        if (output.payload.tableValue.api && output.payload.tableValue.opaque) {
          map = (SHMap *)output.payload.tableValue.opaque;
          map->clear();
        } else {
          varFree(output);
          output.valueType = nextType;
        }
      }

      if (!map) {
        map = new SHMap();
        output.payload.tableValue.api = &GetGlobals().TableInterface;
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
        // TODO improve this, avoid allocations
        SHVar tmp{};
        deserialize(read, tmp);
        auto &dst = (*map)[keyBuf];
        dst = tmp;
        varFree(tmp);
      }
      break;
    }
    case SHType::Set: {
      SHHashSet *set = nullptr;

      if (recycle) {
        if (output.payload.setValue.api && output.payload.setValue.opaque) {
          set = (SHHashSet *)output.payload.setValue.opaque;
          set->clear();
        } else {
          varFree(output);
          output.valueType = nextType;
        }
      }

      if (!set) {
        set = new SHHashSet();
        output.payload.setValue.api = &GetGlobals().SetInterface;
        output.payload.setValue.opaque = set;
      }

      uint64_t len;
      read((uint8_t *)&len, sizeof(uint64_t));
      for (uint64_t i = 0; i < len; i++) {
        // TODO improve this, avoid allocations
        SHVar dst{};
        deserialize(read, dst);
        (*set).emplace(dst);
        varFree(dst);
      }
      break;
    }
    case SHType::Image: {
      size_t currentSize = 0;
      if (recycle) {
        auto bpp = 1;
        if ((output.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
          bpp = 2;
        else if ((output.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
          bpp = 4;
        currentSize =
            output.payload.imageValue.channels * output.payload.imageValue.height * output.payload.imageValue.width * bpp;
      }

      read((uint8_t *)&output.payload.imageValue.channels, sizeof(output.payload.imageValue.channels));
      read((uint8_t *)&output.payload.imageValue.flags, sizeof(output.payload.imageValue.flags));
      read((uint8_t *)&output.payload.imageValue.width, sizeof(output.payload.imageValue.width));
      read((uint8_t *)&output.payload.imageValue.height, sizeof(output.payload.imageValue.height));

      auto pixsize = 1;
      if ((output.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
        pixsize = 2;
      else if ((output.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
        pixsize = 4;

      size_t size =
          output.payload.imageValue.channels * output.payload.imageValue.height * output.payload.imageValue.width * pixsize;

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
    case SHType::Audio: {
      size_t currentSize = 0;
      if (recycle) {
        currentSize = output.payload.audioValue.nsamples * output.payload.audioValue.channels * sizeof(float);
      }

      read((uint8_t *)&output.payload.audioValue.nsamples, sizeof(output.payload.audioValue.nsamples));
      read((uint8_t *)&output.payload.audioValue.channels, sizeof(output.payload.audioValue.channels));
      read((uint8_t *)&output.payload.audioValue.sampleRate, sizeof(output.payload.audioValue.sampleRate));

      size_t size = output.payload.audioValue.nsamples * output.payload.audioValue.channels * sizeof(float);

      if (currentSize > 0 && currentSize < size) {
        // delete first & alloc
        delete[] output.payload.audioValue.samples;
        output.payload.audioValue.samples = new float[output.payload.audioValue.nsamples * output.payload.audioValue.channels];
      } else if (currentSize == 0) {
        // just alloc
        output.payload.audioValue.samples = new float[output.payload.audioValue.nsamples * output.payload.audioValue.channels];
      }

      read((uint8_t *)output.payload.audioValue.samples, size);
      break;
    }
    case SHType::ShardRef: {
      Shard *blk;
      uint32_t len;
      read((uint8_t *)&len, sizeof(uint32_t));
      std::vector<char> buf;
      buf.resize(len + 1);
      read((uint8_t *)&buf[0], len);
      buf[len] = 0;
      blk = createShard(&buf[0]);
      if (!blk) {
        throw shards::SHException("Shard not found! name: " + std::string(&buf[0]));
      }
      // validate the hash of the shard
      uint32_t crc;
      read((uint8_t *)&crc, sizeof(uint32_t));
      if (blk->hash(blk) != crc) {
        throw shards::SHException("Shard hash mismatch, the serialized version is "
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
        SHVar tmp{};
        deserialize(read, tmp);
        blk->setParam(blk, idx, &tmp);
        varFree(tmp);
      }
      if (blk->setState) {
        SHVar state{};
        deserialize(read, state);
        blk->setState(blk, &state);
        varFree(state);
      }
      output.payload.shardValue = blk;
      break;
    }
    case SHType::Wire: {
      uint32_t len;
      read((uint8_t *)&len, sizeof(uint32_t));
      std::vector<char> buf;
      buf.resize(len + 1);
      read((uint8_t *)&buf[0], len);
      buf[len] = 0;

      // search if we already have this wire!
      auto cit = wires.find(&buf[0]);
      if (cit != wires.end()) {
        SHLOG_TRACE("Skipping deserializing wire: {}", SHWire::sharedFromRef(cit->second)->name);
        output.payload.wireValue = SHWire::addRef(cit->second);
        break;
      }

      auto wire = SHWire::make(&buf[0]);
      output.payload.wireValue = wire->newRef();
      wires.emplace(wire->name, SHWire::addRef(output.payload.wireValue));
      SHLOG_TRACE("Deserializing wire: {}", wire->name);
      read((uint8_t *)&wire->looped, 1);
      read((uint8_t *)&wire->unsafe, 1);
      // shards len
      read((uint8_t *)&len, sizeof(uint32_t));
      // shards
      for (uint32_t i = 0; i < len; i++) {
        SHVar shardVar{};
        deserialize(read, shardVar);
        assert(shardVar.valueType == ShardRef);
        wire->addShard(shardVar.payload.shardValue);
        // blow's owner is the wire
      }
      break;
    }
    case SHType::Object: {
      int64_t id;
      read((uint8_t *)&id, sizeof(int64_t));
      uint64_t len;
      read((uint8_t *)&len, sizeof(uint64_t));
      if (len > 0) {
        auto it = GetGlobals().ObjectTypesRegister.find(id);
        if (it != GetGlobals().ObjectTypesRegister.end()) {
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
          output.flags |= SHVAR_FLAGS_USES_OBJINFO;
          output.objectInfo = &info;
          // up ref count also, not included in deserialize op!
          if (info.reference) {
            info.reference(output.payload.objectValue);
          }
        } else {
          throw shards::SHException("Failed to find object type in registry.");
        }
      }
      break;
    }
    }
  }

  template <class BinaryWriter> size_t serialize(const SHVar &input, BinaryWriter &write) {
    size_t total = 0;
    write((const uint8_t *)&input.valueType, sizeof(input.valueType));
    total += sizeof(input.valueType);
    switch (input.valueType) {
    case SHType::None:
    case SHType::EndOfBlittableTypes:
    case SHType::Any:
      break;
    case SHType::Enum:
      write((const uint8_t *)&input.payload, sizeof(int32_t) * 3);
      total += sizeof(int32_t) * 3;
      break;
    case SHType::Bool:
      write((const uint8_t *)&input.payload, sizeof(SHBool));
      total += sizeof(SHBool);
      break;
    case SHType::Int:
      write((const uint8_t *)&input.payload, sizeof(SHInt));
      total += sizeof(SHInt);
      break;
    case SHType::Int2:
      write((const uint8_t *)&input.payload, sizeof(SHInt2));
      total += sizeof(SHInt2);
      break;
    case SHType::Int3:
      write((const uint8_t *)&input.payload, sizeof(SHInt3));
      total += sizeof(SHInt3);
      break;
    case SHType::Int4:
      write((const uint8_t *)&input.payload, sizeof(SHInt4));
      total += sizeof(SHInt4);
      break;
    case SHType::Int8:
      write((const uint8_t *)&input.payload, sizeof(SHInt8));
      total += sizeof(SHInt8);
      break;
    case SHType::Int16:
      write((const uint8_t *)&input.payload, sizeof(SHInt16));
      total += sizeof(SHInt16);
      break;
    case SHType::Float:
      write((const uint8_t *)&input.payload, sizeof(SHFloat));
      total += sizeof(SHFloat);
      break;
    case SHType::Float2:
      write((const uint8_t *)&input.payload, sizeof(SHFloat2));
      total += sizeof(SHFloat2);
      break;
    case SHType::Float3:
      write((const uint8_t *)&input.payload, sizeof(SHFloat3));
      total += sizeof(SHFloat3);
      break;
    case SHType::Float4:
      write((const uint8_t *)&input.payload, sizeof(SHFloat4));
      total += sizeof(SHFloat4);
      break;
    case SHType::Color:
      write((const uint8_t *)&input.payload, sizeof(SHColor));
      total += sizeof(SHColor);
      break;
    case SHType::Bytes:
      write((const uint8_t *)&input.payload.bytesSize, sizeof(input.payload.bytesSize));
      total += sizeof(input.payload.bytesSize);
      write((const uint8_t *)input.payload.bytesValue, input.payload.bytesSize);
      total += input.payload.bytesSize;
      break;
    case SHType::Array: {
      write((const uint8_t *)&input.innerType, sizeof(input.innerType));
      total += sizeof(input.innerType);
      write((const uint8_t *)&input.payload.arrayValue.len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      auto size = input.payload.arrayValue.len * sizeof(SHVarPayload);
      write((const uint8_t *)&input.payload.arrayValue.elements[0], size);
      total += size;
    } break;
    case SHType::Path:
    case SHType::String:
    case SHType::ContextVar: {
      uint32_t len = input.payload.stringLen > 0 || input.payload.stringValue == nullptr
                         ? input.payload.stringLen
                         : uint32_t(strlen(input.payload.stringValue));
      write((const uint8_t *)&len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      write((const uint8_t *)input.payload.stringValue, len);
      total += len;
      break;
    }
    case SHType::Seq: {
      uint32_t len = input.payload.seqValue.len;
      write((const uint8_t *)&len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      for (uint32_t i = 0; i < len; i++) {
        total += serialize(input.payload.seqValue.elements[i], write);
      }
      break;
    }
    case SHType::Table: {
      if (input.payload.tableValue.api && input.payload.tableValue.opaque) {
        auto &t = input.payload.tableValue;
        uint64_t len = (uint64_t)t.api->tableSize(t);
        write((const uint8_t *)&len, sizeof(uint64_t));
        total += sizeof(uint64_t);
        SHTableIterator tit;
        t.api->tableGetIterator(t, &tit);
        SHString k;
        SHVar v;
        while (t.api->tableNext(t, &tit, &k, &v)) {
          uint32_t klen = strlen(k);
          write((const uint8_t *)&klen, sizeof(uint32_t));
          total += sizeof(uint32_t);
          write((const uint8_t *)k, klen);
          total += klen;
          total += serialize(v, write);
        }
      } else {
        uint64_t none = 0;
        write((const uint8_t *)&none, sizeof(uint64_t));
        total += sizeof(uint64_t);
      }
      break;
    }
    case SHType::Set: {
      if (input.payload.setValue.api && input.payload.setValue.opaque) {
        auto &s = input.payload.setValue;
        uint64_t len = (uint64_t)s.api->setSize(s);
        write((const uint8_t *)&len, sizeof(uint64_t));
        total += sizeof(uint64_t);
        SHSetIterator sit;
        s.api->setGetIterator(s, &sit);
        SHVar v;
        while (s.api->setNext(s, &sit, &v)) {
          total += serialize(v, write);
        }
      } else {
        uint64_t none = 0;
        write((const uint8_t *)&none, sizeof(uint64_t));
        total += sizeof(uint64_t);
      }
      break;
    }
    case SHType::Image: {
      auto pixsize = 1;
      if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
        pixsize = 2;
      else if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
        pixsize = 4;
      write((const uint8_t *)&input.payload.imageValue.channels, sizeof(input.payload.imageValue.channels));
      total += sizeof(input.payload.imageValue.channels);
      write((const uint8_t *)&input.payload.imageValue.flags, sizeof(input.payload.imageValue.flags));
      total += sizeof(input.payload.imageValue.flags);
      write((const uint8_t *)&input.payload.imageValue.width, sizeof(input.payload.imageValue.width));
      total += sizeof(input.payload.imageValue.width);
      write((const uint8_t *)&input.payload.imageValue.height, sizeof(input.payload.imageValue.height));
      total += sizeof(input.payload.imageValue.height);
      auto size = input.payload.imageValue.channels * input.payload.imageValue.height * input.payload.imageValue.width * pixsize;
      write((const uint8_t *)input.payload.imageValue.data, size);
      total += size;
      break;
    }
    case SHType::Audio: {
      write((const uint8_t *)&input.payload.audioValue.nsamples, sizeof(input.payload.audioValue.nsamples));
      total += sizeof(input.payload.audioValue.nsamples);

      write((const uint8_t *)&input.payload.audioValue.channels, sizeof(input.payload.audioValue.channels));
      total += sizeof(input.payload.audioValue.channels);

      write((const uint8_t *)&input.payload.audioValue.sampleRate, sizeof(input.payload.audioValue.sampleRate));
      total += sizeof(input.payload.audioValue.sampleRate);

      auto size = input.payload.audioValue.nsamples * input.payload.audioValue.channels * sizeof(float);

      write((const uint8_t *)input.payload.audioValue.samples, size);
      total += size;
      break;
    }
    case SHType::ShardRef: {
      auto blk = input.payload.shardValue;
      // name
      auto name = blk->name(blk);
      uint32_t len = uint32_t(strlen(name));
      write((const uint8_t *)&len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      write((const uint8_t *)name, len);
      total += len;
      // serialize the hash of the shard as well
      auto crc = blk->hash(blk);
      write((const uint8_t *)&crc, sizeof(uint32_t));
      total += sizeof(uint32_t);
      // params
      // well, this is bad and should be fixed somehow at some point
      // we are creating a shard just to compare to figure default values
      auto model =
          defaultShards.emplace(name, std::shared_ptr<Shard>(createShard(name), [](Shard *shard) { shard->destroy(shard); }))
              .first->second.get();
      if (!model) {
        SHLOG_FATAL("Could not create shard: {}.", name);
      }
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
    case SHType::Wire: {
      auto sc = SHWire::sharedFromRef(input.payload.wireValue);
      auto wire = sc.get();

      { // Name
        uint32_t len = uint32_t(wire->name.size());
        write((const uint8_t *)&len, sizeof(uint32_t));
        total += sizeof(uint32_t);
        write((const uint8_t *)wire->name.c_str(), len);
        total += len;
      }

      // stop here if we had it already
      if (wires.count(wire->name) > 0) {
        SHLOG_TRACE("Skipping serializing wire: {}", wire->name);
        break;
      }

      SHLOG_TRACE("Serializing wire: {}", wire->name);
      wires.emplace(wire->name, SHWire::addRef(input.payload.wireValue));

      { // Looped & Unsafe
        write((const uint8_t *)&wire->looped, 1);
        total += 1;
        write((const uint8_t *)&wire->unsafe, 1);
        total += 1;
      }
      { // Shards len
        uint32_t len = uint32_t(wire->shards.size());
        write((const uint8_t *)&len, sizeof(uint32_t));
        total += sizeof(uint32_t);
      }
      // Shards
      for (auto shard : wire->shards) {
        SHVar shardVar{};
        shardVar.valueType = SHType::ShardRef;
        shardVar.payload.shardValue = shard;
        total += serialize(shardVar, write);
      }
      break;
    }
    case SHType::Object: {
      int64_t id = (int64_t)input.payload.objectVendorId << 32 | input.payload.objectTypeId;
      write((const uint8_t *)&id, sizeof(int64_t));
      total += sizeof(int64_t);
      if ((input.flags & SHVAR_FLAGS_USES_OBJINFO) == SHVAR_FLAGS_USES_OBJINFO && input.objectInfo &&
          input.objectInfo->serialize) {
        size_t len = 0;
        uint8_t *data = nullptr;
        SHPointer handle = nullptr;
        if (!input.objectInfo->serialize(input.payload.objectValue, &data, &len, &handle)) {
          throw shards::SHException("Failed to serialize custom object variable!");
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

template <typename T> struct WireDoppelgangerPool {
  WireDoppelgangerPool(SHWireRef master) {
    auto vwire = shards::Var(master);
    std::stringstream stream;
    Writer w(stream);
    Serialization serializer;
    serializer.serialize(vwire, w);
    _wireStr = stream.str();
  }

  // notice users should stop wires themselves, we might want wires to persist
  // after this object lifetime
  void stopAll() {
    for (auto &item : _pool) {
      stop(item->wire.get());
      _avail.emplace(item);
    }
  }

  template <class Composer> std::shared_ptr<T> acquire(Composer &composer) {
    if (_avail.size() == 0) {
      Serialization serializer;
      std::stringstream stream(_wireStr);
      Reader r(stream);
      SHVar vwire{};
      serializer.deserialize(r, vwire);
      auto wire = SHWire::sharedFromRef(vwire.payload.wireValue);
      auto fresh = _pool.emplace_back(std::make_shared<T>());
      fresh->wire = wire;
      composer.compose(wire.get());
      fresh->wire->name = fresh->wire->name + "-" + std::to_string(_pool.size());
      return fresh;
    } else {
      auto res = _avail.extract(_avail.begin());
      return res.value();
    }
  }

  void release(std::shared_ptr<T> wire) { _avail.emplace(wire); }

private:
  struct Writer {
    std::stringstream &stream;
    Writer(std::stringstream &stream) : stream(stream) {}
    void operator()(const uint8_t *buf, size_t size) { stream.write((const char *)buf, size); }
  };

  struct Reader {
    std::stringstream &stream;
    Reader(std::stringstream &stream) : stream(stream) {}
    void operator()(uint8_t *buf, size_t size) { stream.read((char *)buf, size); }
  };

  // keep our pool in a deque in order to keep them alive
  // so users don't have to worry about lifetime
  // just release when possible
  std::deque<std::shared_ptr<T>> _pool;
  std::unordered_set<std::shared_ptr<T>> _avail;
  std::string _wireStr;
};

#ifdef __EMSCRIPTEN__
template <typename T> inline T emscripten_wait(SHContext *context, emscripten::val promise) {
  const static emscripten::val futs = emscripten::val::global("ShardsBonder");
  emscripten::val fut = futs.new_(promise);
  fut.call<void>("run");

  while (!fut["finished"].as<bool>()) {
    suspend(context, 0.0);
  }

  if (fut["hadErrors"].as<bool>()) {
    throw ActivationError("A javascript async task has failed, check the "
                          "console for more informations.");
  }

  return fut["result"].as<T>();
}
#endif

#ifdef __EMSCRIPTEN__
// limit to 4 under emscripten
extern Shared<boost::asio::thread_pool, int, 4> SharedThreadPool;
#else
extern Shared<boost::asio::thread_pool> SharedThreadPool;
#endif

template <typename FUNC, typename CANCELLATION>
inline SHVar awaitne(SHContext *context, FUNC &&func, CANCELLATION &&cancel) noexcept {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  return func();
#else
  std::exception_ptr exp = nullptr;
  SHVar res{};
  std::atomic_bool complete = false;

  boost::asio::dispatch(shards::SharedThreadPool(), [&]() {
    try {
      res = func();
    } catch (...) {
      exp = std::current_exception();
    }
    complete = true;
  });

  while (!complete && context->shouldContinue()) {
    if (shards::suspend(context, 0) != SHWireState::Continue)
      break;
  }

  if (unlikely(!complete)) {
    cancel();
    while (!complete) {
      std::this_thread::yield();
    }
  }

  if (exp) {
    try {
      std::rethrow_exception(exp);
    } catch (const std::exception &e) {
      context->cancelFlow(e.what());
    } catch (...) {
      context->cancelFlow("foreign exception failure");
    }
  }

  return res;
#endif
}

template <typename FUNC, typename CANCELLATION> inline void await(SHContext *context, FUNC &&func, CANCELLATION &&cancel) {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  func();
#else
  std::exception_ptr exp = nullptr;
  std::atomic_bool complete = false;

  boost::asio::dispatch(shards::SharedThreadPool(), [&]() {
    try {
      func();
    } catch (...) {
      exp = std::current_exception();
    }
    complete = true;
  });

  while (!complete && context->shouldContinue()) {
    if (shards::suspend(context, 0) != SHWireState::Continue)
      break;
  }

  if (unlikely(!complete)) {
    cancel();
    while (!complete) {
      std::this_thread::yield();
    }
  }

  if (exp) {
    std::rethrow_exception(exp);
  }
#endif
}
} // namespace shards

#endif // SH_CORE_RUNTIME

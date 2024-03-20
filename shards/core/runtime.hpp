/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_RUNTIME
#define SH_CORE_RUNTIME

// must go first
#include <shards/shards.h>
#include <type_traits>

#if _WIN32
#include <winsock2.h>
#endif

#include <string.h> // memset

#include "shards_macros.hpp"
#include "foundation.hpp"
#include "inline.hpp"
#include "utils.hpp"

#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/container/small_vector.hpp>

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

#ifdef SH_USE_TSAN
extern "C" {
void *__tsan_get_current_fiber(void);
void *__tsan_create_fiber(unsigned flags);
void __tsan_destroy_fiber(void *fiber);
void __tsan_switch_to_fiber(void *fiber, unsigned flags);
void __tsan_set_fiber_name(void *fiber, const char *name);
const unsigned __tsan_switch_to_fiber_no_sync = 1 << 0;
}
#define TSANCoroEnter(wire)              \
  {                                      \
    if (!getCoroWireStack2().empty()) {  \
      TracyFiberLeave;                   \
    }                                    \
    TracyFiberEnter(wire->name.c_str()); \
    getCoroWireStack2().push_back(wire); \
  }
#define TSANCoroExit(wire)                                       \
  {                                                              \
    getCoroWireStack2().pop_back();                              \
    TracyFiberLeave;                                             \
    if (!getCoroWireStack2().empty()) {                          \
      TracyFiberEnter(getCoroWireStack2().back()->name.c_str()); \
    }                                                            \
  }
#else
#define TSANCoroEnter(wire)
#define TSANCoroExit(wire)
#endif

#define XXH_INLINE_ALL
#include <xxhash.h>

#ifndef CUSTOM_XXH3_kSecret
// Applications embedding shards can override this and should.
// TODO add our secret
#define CUSTOM_XXH3_kSecret XXH3_kSecret
#endif

#define SH_SUSPEND(_ctx_, _secs_)                             \
  const auto _suspend_state = shards::suspend(_ctx_, _secs_); \
  if (_suspend_state != SHWireState::Continue)                \
  return shards::Var::Empty

struct SHFlow {
  struct SHWire *wire;
  SHBool paused;
};

struct SHStateSnapshot {
  SHWireState state;
  SHVar flowStorage;
  std::string errorMessage;
};

struct SHContext {
  SHContext(shards::Coroutine *coro, const SHWire *starter, SHFlow *flow) : main(starter), flow(flow), continuation(coro) {
    wireStack.push_back(const_cast<SHWire *>(starter));
  }

  const SHWire *main;
  SHFlow *flow;
  SHContext *parent{nullptr};
  std::vector<SHWire *> wireStack;
  bool onLastResume{false};
  bool onWorkerThread{false};

  std::mutex anyStorageLock;
  std::unordered_map<std::string, std::shared_ptr<entt::any>> anyStorage;

  // Used within the coro& stack! (suspend, etc)
  shards::Coroutine *continuation{nullptr};
  SHDuration next{};

  entt::delegate<void()> meshThreadTask;

  SHWire *rootWire() const { return wireStack.front(); }
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
    state = SHWireState::Error;
    errorMessage = message;
  }

  SHStateSnapshot takeStateSnapshot() {
    return SHStateSnapshot{
        state,
        std::move(flowStorage),
        std::move(errorMessage),
    };
  }

  void restoreStateSnapshot(SHStateSnapshot &&snapshot) {
    errorMessage = std::move(snapshot.errorMessage);
    state = std::move(snapshot.state);
    flowStorage = std::move(snapshot.flowStorage);
  }

  constexpr void rebaseFlow() { state = SHWireState::Rebase; }

  constexpr void continueFlow() { state = SHWireState::Continue; }

  constexpr bool shouldContinue() const { return state == SHWireState::Continue; }

  constexpr bool shouldReturn() const { return state == SHWireState::Return; }

  constexpr bool shouldStop() const { return state == SHWireState::Stop; }

  constexpr bool failed() const { return state == SHWireState::Error; }

  constexpr const std::string &getErrorMessage() { return errorMessage; }

  constexpr SHWireState getState() const { return state; }

  constexpr SHVar getFlowStorage() const { return flowStorage; }

private:
  SHWireState state = SHWireState::Continue; // don't make this atomic! it's used a lot!
  // Used when flow is stopped/restart/return
  // to store the previous result
  SHVar flowStorage{};
  std::string errorMessage;
};

namespace shards {
[[nodiscard]] SHComposeResult composeWire(const std::vector<Shard *> &wire, SHValidationCallback callback, void *userData,
                                          SHInstanceData data);
[[nodiscard]] SHComposeResult composeWire(const Shards wire, SHValidationCallback callback, void *userData, SHInstanceData data);
[[nodiscard]] SHComposeResult composeWire(const SHSeq wire, SHValidationCallback callback, void *userData, SHInstanceData data);
[[nodiscard]] SHComposeResult composeWire(const SHWire *wire, SHValidationCallback callback, void *userData, SHInstanceData data);

bool validateSetParam(Shard *shard, int index, const SHVar &value, SHValidationCallback callback, void *userData);
bool matchTypes(const SHTypeInfo &inputType, const SHTypeInfo &receiverType, bool isParameter, bool strict,
                bool relaxEmptySeqCheck);
void triggerVarValueChange(SHContext *context, const SHVar *name, const SHVar *var);
void triggerVarValueChange(SHWire *wire, const SHVar *name, const SHVar *var);

void installSignalHandlers();

bool isDebuggerPresent();

#ifdef SH_COMPRESSED_STRINGS
void decompressStrings();
#endif

template <typename T> struct AnyStorage {
private:
  std::shared_ptr<entt::any> _anyStorage;
  T *_ptr{};

public:
  AnyStorage() = default;
  AnyStorage(std::shared_ptr<entt::any> &&any) : _anyStorage(any) { _ptr = &entt::any_cast<T &>(*_anyStorage.get()); }
  operator bool() const { return _ptr; }
  T *operator->() const { return _ptr; }
  operator T &() const { return *_ptr; }
};

template <typename TInit, typename T = decltype((*(TInit *)0)()), typename C>
AnyStorage<T> getOrCreateAnyStorage(C *context, const std::string &storageKey, TInit init) {
  std::unique_lock<std::mutex> l(context->anyStorageLock);
  auto ptr = context->anyStorage[storageKey];
  if (!ptr) {
    // recurse into parent if we have one
    shassert(context->parent != context);
    if (context->parent) {
      l.unlock();
      return getOrCreateAnyStorage<TInit, T>(context->parent, storageKey, init);
    } else {
      ptr = std::make_shared<entt::any>(init());
      context->anyStorage[storageKey] = ptr;
      return ptr;
    }
  } else {
    return ptr;
  }
}

template <typename T, typename C> AnyStorage<T> getOrCreateAnyStorage(C *context, const std::string &storageKey) {
  auto v = []() { return std::in_place_type_t<T>(); };
  return getOrCreateAnyStorage<decltype(v), T>(context, storageKey, v);
}

FLATTEN ALWAYS_INLINE inline SHVar activateShard(Shard *blk, SHContext *context, const SHVar &input) {
  ZoneScoped;
  ZoneName(blk->name(blk), blk->nameLength);

  SHVar output;
  if (!activateShardInline(blk, context, input, output))
    output = blk->activate(blk, context, &input);
  return output;
}

SHRunWireOutput runWire(SHWire *wire, SHContext *context, const SHVar &wireInput);

inline SHRunWireOutput runSubWire(SHWire *wire, SHContext *context, const SHVar &input) {
  // push to wire stack
  context->wireStack.push_back(wire);
  DEFER({ context->wireStack.pop_back(); });
  auto runRes = shards::runWire(wire, context, input);
  return runRes;
}

void run(SHWire *wire, SHFlow *flow, shards::Coroutine *coro);

#ifdef TRACY_ENABLE
// Defined in the gfx rust crate
//   used to initialize tracy on the rust side, since it required special intialization (C++ doesn't)
//   but since we link to the dll, we can use it from C++ too
extern "C" void gfxTracyInit();

struct GlobalTracy {
  GlobalTracy() { gfxTracyInit(); }

  // just need to fool the compiler
  constexpr bool isInitialized() const { return true; }
};

extern GlobalTracy &GetTracy();
#endif

#ifdef TRACY_FIBERS
std::vector<SHWire *> &getCoroWireStack();
#endif

#if SH_DEBUG_THREAD_NAMES
#define SH_CORO_RESUMED(_wire) \
  { shards::pushThreadName(fmt::format("Wire \"{}\"", (_wire)->name)); }
#define SH_CORO_SUSPENDED(_wire) \
  { shards::popThreadName(); }
#define SH_CORO_EXT_RESUME(_wire)                                                 \
  {                                                                               \
    shards::pushThreadName(fmt::format("<resuming wire> \"{}\"", (_wire)->name)); \
    TracyCoroEnter(wire);                                                         \
  }
#define SH_CORO_EXT_SUSPEND(_wire) \
  {                                \
    shards::popThreadName();       \
    TracyCoroExit(_wire);          \
  }
#else
#define SH_CORO_RESUMED(_wire)
#define SH_CORO_SUSPENDED(_)
#define SH_CORO_EXT_RESUME(_) \
  { TracyCoroEnter(wire); }
#define SH_CORO_EXT_SUSPEND(_) \
  { TracyCoroExit(_wire); }
#endif

inline void prepare(SHWire *wire, SHFlow *flow) {
  shassert(!coroutineValid(wire->coro) && "Wire already prepared!");

  auto runner = [wire, flow]() {
#if SH_USE_THREAD_FIBER
    pushThreadName(fmt::format("<suspended wire> \"{}\"", wire->name));
#endif
    run(wire, flow, &wire->coro);
  };

#if SH_CORO_NEED_STACK_MEM
  if (!wire->stackMem) {
    wire->stackMem = new (std::align_val_t{16}) uint8_t[wire->stackSize];
  }
  wire->coro.emplace(SHStackAllocator{wire->stackSize, wire->stackMem});
#else
  wire->coro.emplace();
#endif

  SH_CORO_EXT_RESUME(wire);
  wire->coro->init(runner);
  SH_CORO_EXT_SUSPEND(wire);
}

inline void start(SHWire *wire, SHVar input = {}) {
  if (wire->state != SHWire::State::Prepared) {
    SHLOG_ERROR("Attempted to start a wire ({}) not ready for running!", wire->name);
    return;
  }

  if (!coroutineValid(wire->coro))
    return; // check if not null and bool operator also to see if alive!

  wire->currentInput = input;
  wire->state = SHWire::State::Starting;
}

inline bool isRunning(SHWire *wire) {
  const auto state = wire->state.load(); // atomic
  return state >= SHWire::State::Starting && state <= SHWire::State::IterationEnded;
}

template <bool IsCleanupContext = false> inline void tick(SHWire *wire, SHDuration now) {
  ZoneScoped;
  ZoneName(wire->name.c_str(), wire->name.size());

  while (true) {
    bool canRun = false;
    if constexpr (IsCleanupContext) {
      canRun = true;
    } else {
      canRun = (isRunning(wire) && now >= wire->context->next) || unlikely(wire->context && wire->context->onLastResume);
    }

    if (canRun) {
      shassert(wire->context && "Wire has no context!");
      shassert(coroutineValid(wire->coro) && "Wire has no coroutine!");

      SH_CORO_EXT_RESUME(wire);
      coroutineResume(wire->coro);
      SH_CORO_EXT_SUSPEND(wire);

      // if we have a task to run, run it and resume coro without yielding to caller
      if (unlikely(wire->context && (bool)wire->context->meshThreadTask)) {
        shassert(wire->context->parent == nullptr && "Mesh thread task should only be called on root context!");
        wire->context->meshThreadTask();
        wire->context->meshThreadTask.reset();
        // And continue in order to resume the coroutine
      } else {
        // Yield to caller if no main thread task
        return;
      }
    } else {
      // We can't run, so we yield to caller
      return;
    }
  }
}

inline bool stop(SHWire *wire, SHVar *result = nullptr, SHContext *currentContext = nullptr) {
  if (wire->state == SHWire::State::Stopped) {
    // Clone the results if we need them
    if (result)
      cloneVar(*result, wire->finishedOutput);

    return true;
  }

  bool stopping = false; // <- expected
  // if exchange fails, we are already stopping
  if (!const_cast<SHWire *>(wire)->stopping.compare_exchange_strong(stopping, true))
    return true;
  DEFER({ wire->stopping = false; });

  SHLOG_TRACE("stopping wire: {}, has-coro: {}, state: {}", wire->name, bool(wire->coro),
              magic_enum::enum_name<SHWire::State>(wire->state));

  if (coroutineValid(wire->coro)) {
    // Run until exit if alive, need to propagate to all suspended shards!
    if (coroutineValid(wire->coro) && wire->state > SHWire::State::Stopped && wire->state < SHWire::State::Failed) {
      // set abortion flag, we always have a context in this case
      wire->context->stopFlow(shards::Var::Empty);
      wire->context->onLastResume = true;

      // BIG Warning: wire->context existed in the coro stack!!!
      // after this resume wire->context is trash!

      // Another issue, if we resume from current context to current context we dead lock here!!
      if (currentContext && currentContext == wire->context) {
        SHLOG_WARNING("Trying to stop wire {} from the same context it's running in!", wire->name);
      } else {
        shards::tick<true>(wire, SHDuration{});
      }
    }

    // delete also the coro ptr
    wire->coro.reset();
  } else {
    // if we had a coro this will run inside it!
    wire->cleanup(true);

    // let's not forget to call events, those are called inside coro handler for the above case
    wire->dispatcher.trigger(SHWire::OnStopEvent{wire});
  }

  // return true if we ended, as in we did our job
  auto res = wire->state == SHWire::State::Ended;

  wire->state = SHWire::State::Stopped;
  wire->currentInput.reset();

  // Clone the results if we need them
  if (result)
    cloneVar(*result, wire->finishedOutput);

  return res;
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
    for (auto &shInfo : GetGlobals().RunLoopHooks) {
      if (shInfo.second) {
        shInfo.second();
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

  if (runCallbacks)
    FrameMarkNamed("Main Shards Yield");
}

struct RuntimeCallbacks {
  // TODO, turn them into filters maybe?
  virtual void registerShard(const char *fullName, SHShardConstructor constructor) = 0;
  virtual void registerObjectType(int32_t vendorId, int32_t typeId, SHObjectInfo info) = 0;
  virtual void registerEnumType(int32_t vendorId, int32_t typeId, SHEnumInfo info) = 0;
};
}; // namespace shards

struct SHMesh : public std::enable_shared_from_this<SHMesh> {
  static constexpr uint32_t TypeId = 'brcM';
  static inline shards::Type MeshType{{SHType::Object, {.object = {.vendorId = shards::CoreCC, .typeId = TypeId}}}};
  static inline shards::ObjectVar<std::shared_ptr<SHMesh>> MeshVar{"Mesh", shards::CoreCC, TypeId};

  static std::shared_ptr<SHMesh> make() { return std::shared_ptr<SHMesh>(new SHMesh()); }

  static std::shared_ptr<SHMesh> *makePtr() { return new std::shared_ptr<SHMesh>(new SHMesh()); }

  ~SHMesh() { terminate(); }

  void compose(const std::shared_ptr<SHWire> &wire, SHVar input = shards::Var::Empty) {
    ZoneScoped;

    SHLOG_TRACE("Composing wire {}", wire->name);

    if (wire->warmedUp) {
      SHLOG_ERROR("Attempted to Pre-composing a wire multiple times, wire: {}", wire->name);
      throw shards::SHException("Multiple wire Pre-composing");
    }

    wire->mesh = shared_from_this();

    // this is to avoid recursion during compose
    visitedWires.clear();

    wire->isRoot = true;
    // remove when done here
    DEFER(wire->isRoot = false);

    // compose the wire
    SHInstanceData data = instanceData;
    data.wire = wire.get();
    data.inputType = shards::deriveTypeInfo(input, data);
    auto validation = shards::composeWire(
        wire.get(),
        [](const Shard *errorShard, SHStringWithLen errorTxt, bool nonfatalWarning, void *userData) {
          auto blk = const_cast<Shard *>(errorShard);
          if (!nonfatalWarning) {
            throw shards::ComposeError(std::string(errorTxt.string, errorTxt.len) +
                                       ", input shard: " + std::string(blk->name(blk)));
          } else {
            SHLOG_INFO("Validation warning: {} input shard: {}", errorTxt, blk->name(blk));
          }
        },
        this, data);
    shards::arrayFree(validation.exposedInfo);
    shards::arrayFree(validation.requiredInfo);
    shards::freeDerivedInfo(data.inputType);

    SHLOG_TRACE("Wire {} composed", wire->name);
  }

  struct EmptyObserver {
    void before_compose(SHWire *wire) {}
    void before_tick(SHWire *wire) {}
    void before_stop(SHWire *wire) {}
    void before_prepare(SHWire *wire) {}
    void before_start(SHWire *wire) {}
  };

  template <class Observer>
  void schedule(Observer observer, const std::shared_ptr<SHWire> &wire, SHVar input = shards::Var::Empty, bool compose = true) {
    ZoneScoped;

    SHLOG_TRACE("Scheduling wire {}", wire->name);

    if (wire->warmedUp || scheduled.count(wire) > 0) {
      SHLOG_ERROR("Attempted to schedule a wire multiple times, wire: {}", wire->name);
      throw shards::SHException("Multiple wire schedule");
    }

    wire->mesh = shared_from_this();

    observer.before_compose(wire.get());
    if (compose) {
      // this is to avoid recursion during compose
      visitedWires.clear();

      wire->isRoot = true;
      // remove when done here
      DEFER(wire->isRoot = false);

      // compose the wire
      SHInstanceData data = instanceData;
      data.wire = wire.get();
      data.inputType = shards::deriveTypeInfo(input, data);
      auto validation = shards::composeWire(
          wire.get(),
          [](const Shard *errorShard, SHStringWithLen errorTxt, bool nonfatalWarning, void *userData) {
            auto blk = const_cast<Shard *>(errorShard);
            if (!nonfatalWarning) {
              throw shards::ComposeError(std::string(errorTxt.string, errorTxt.len) +
                                         ", input shard: " + std::string(blk->name(blk)));
            } else {
              SHLOG_INFO("Validation warning: {} input shard: {}", errorTxt, blk->name(blk));
            }
          },
          this, data);
      shards::arrayFree(validation.exposedInfo);
      shards::arrayFree(validation.requiredInfo);
      shards::freeDerivedInfo(data.inputType);

      SHLOG_TRACE("Wire {} composed", wire->name);
    } else {
      SHLOG_TRACE("Wire {} skipped compose", wire->name);
    }

    observer.before_prepare(wire.get());
    // create a flow as well
    auto &flow = _flowPool.emplace_back();
    flow.wire = wire.get();
    shards::prepare(wire.get(), &flow);

    // wire might fail on warmup during prepare
    if (wire->state == SHWire::State::Failed) {
      throw shards::SHException(fmt::format("Wire {} failed during prepare", wire->name));
    }

    observer.before_start(wire.get());
    shards::start(wire.get(), input);

    scheduled.insert(wire);

    SHLOG_TRACE("Wire {} scheduled", wire->name);
  }

  void schedule(const std::shared_ptr<SHWire> &wire, SHVar input = shards::Var::Empty, bool compose = true) {
    EmptyObserver obs;
    schedule(obs, wire, input, compose);
  }

  template <class Observer> bool tick(Observer observer) {
    ZoneScoped;

    auto noErrors = true;
    _errors.clear();
    _failedWires.clear();

    if (shards::GetGlobals().SigIntTerm > 0) {
      terminate();
    } else {
      SHDuration now = SHClock::now().time_since_epoch();
      for (auto it = _flowPool.begin(); it != _flowPool.end();) {
        auto &flow = *it;
        if (flow.paused) {
          ++it;
          continue; // simply skip
        }

        observer.before_tick(flow.wire);

        shards::tick(flow.wire, now);

        if (unlikely(!shards::isRunning(flow.wire))) {
          if (flow.wire->finishedError.size() > 0) {
            _errors.emplace_back(flow.wire->finishedError);
          }

          if (flow.wire->state == SHWire::State::Failed) {
            _failedWires.emplace_back(flow.wire);
            noErrors = false;
          }

          observer.before_stop(flow.wire);
          if (!shards::stop(flow.wire)) {
            noErrors = false;
          }

          // stop should have done the following:
          SHLOG_TRACE("Wire {} ended while ticking", flow.wire->name);
          shassert(visitedWires.count(flow.wire) == 0 && "Wire still in visitedWires!");
          shassert(scheduled.count(flow.wire->shared_from_this()) == 0 && "Wire still in scheduled!");
          shassert(flow.wire->mesh.expired() && "Wire still has a mesh!");

          it = _flowPool.erase(it);
        } else {
          ++it;
        }
      }
    }

    return noErrors;
  }

  bool tick() {
    EmptyObserver obs;
    return tick(obs);
  }

  void clear() {
    // clear all wires!
    boost::container::small_vector<std::shared_ptr<SHWire>, 16> toStop;

    // scheduled might not be the full picture!
    for (auto flow : _flowPool) {
      toStop.emplace_back(flow.wire->shared_from_this());
    }

    // now add scheduled, notice me might have duplicates!
    for (auto wire : scheduled) {
      toStop.emplace_back(wire);
    }

    // remove dupes
    std::sort(toStop.begin(), toStop.end());
    toStop.erase(std::unique(toStop.begin(), toStop.end()), toStop.end());

    for (auto wire : toStop) {
      shards::stop(wire.get());
      // stop should have done the following:
      shassert(visitedWires.count(wire.get()) == 0 && "Wire still in visitedWires!");
      shassert(scheduled.count(wire) == 0 && "Wire still in scheduled!");
      shassert(wire->mesh.expired() && "Wire still has a mesh!");
    }

    _flowPool.clear();

    // release all wires
    scheduled.clear();

    // do this here just in case as well as we might be not using schedule directly!
    visitedWires.clear();

    // find dangling variables and notice
    for (auto var : variables) {
      if (var.second.refcount > 0) {
        SHLOG_ERROR("Found a dangling global variable: {}", var.first);
      }
    }
    variables.clear();
  }

  void terminate() {
    clear();

    // whichever shard uses refs must clean them
    refs.clear();

    // finally clear storage
    anyStorage.clear();
  }

  void remove(const std::shared_ptr<SHWire> &wire) {
    shards::stop(wire.get());
    // stop should have done the following:
    shassert(visitedWires.count(wire.get()) == 0 && "Wire still in visitedWires!");
    shassert(scheduled.count(wire) == 0 && "Wire still in scheduled!");
    shassert(wire->mesh.expired() && "Wire still has a mesh!");

    // Erase-Remove Idiom
    _flowPool.erase(
        std::remove_if(_flowPool.begin(), _flowPool.end(), [wire](const auto &flow) { return flow.wire == wire.get(); }),
        _flowPool.end());
  }

  bool empty() { return _flowPool.empty(); }

  const std::vector<std::string> &errors() { return _errors; }

  const std::vector<SHWire *> &failedWires() { return _failedWires; }

  std::unordered_map<SHWire *, SHTypeInfo> visitedWires;
  std::mutex visitedWiresMutex;

  std::unordered_set<std::shared_ptr<SHWire>> scheduled;

  SHInstanceData instanceData{};

  std::mutex anyStorageLock;
  std::unordered_map<std::string, std::shared_ptr<entt::any>> anyStorage;
  SHMesh *parent{nullptr};

  // up to the users to call .update on this, we internally use just "trigger", which is instant
  mutable entt::dispatcher dispatcher{};

  SHVar &getVariable(const SHStringWithLen name) {
    auto key = shards::OwnedVar::Foreign(name); // copy on write
    return variables[key];
  }

  constexpr auto &getVariables() { return variables; }

  void setMetadata(SHVar *var, SHExposedTypeInfo info) {
    auto it = variablesMetadata.find(var);
    if (it != variablesMetadata.end()) {
      if (info != it->second) {
        SHLOG_WARNING("Metadata for global variable {} already exists and is different!", info.name);
      }
    }
    variablesMetadata[var] = info;
  }

  void releaseMetadata(SHVar *var) {
    if (var->refcount == 0) {
      variablesMetadata.erase(var);
    }
  }

  std::optional<SHExposedTypeInfo> getMetadata(SHVar *var) {
    auto it = variablesMetadata.find(var);
    if (it != variablesMetadata.end()) {
      return it->second;
    } else {
      return std::nullopt;
    }
  }

  void addRef(const SHStringWithLen name, SHVar *var) {
    shassert(((var->flags & SHVAR_FLAGS_REF_COUNTED) == SHVAR_FLAGS_REF_COUNTED && var->refcount > 0) ||
             (var->flags & SHVAR_FLAGS_EXTERNAL) == SHVAR_FLAGS_EXTERNAL);
    auto key = shards::OwnedVar::Foreign(name); // copy on write
    refs[key] = var;
  }

  std::optional<std::reference_wrapper<SHVar>> getVariableIfExists(const SHStringWithLen name) {
    auto key = shards::OwnedVar::Foreign(name);
    auto it = variables.find(key);
    if (it != variables.end()) {
      return it->second;
    } else {
      return std::nullopt;
    }
  }

  SHVar *getRefIfExists(const SHStringWithLen name) {
    auto key = shards::OwnedVar::Foreign(name);
    auto it = refs.find(key);
    if (it != refs.end()) {
      return it->second;
    } else {
      return nullptr;
    }
  }

  void releaseAllRefs() {
    for (auto ref : refs) {
      shards::releaseVariable(ref.second);
    }
    refs.clear();
  }

  bool hasRef(const SHStringWithLen name) {
    auto key = shards::OwnedVar::Foreign(name);
    return refs.count(key) > 0;
  }

private:
  SHMesh() = default;

  std::unordered_map<shards::OwnedVar, SHVar, std::hash<shards::OwnedVar>, std::equal_to<shards::OwnedVar>,
                     boost::alignment::aligned_allocator<std::pair<const shards::OwnedVar, SHVar>, 16>>
      variables;

  std::unordered_map<SHVar *, SHExposedTypeInfo> variablesMetadata;

  // variables with lifetime managed externally
  std::unordered_map<shards::OwnedVar, SHVar *, std::hash<shards::OwnedVar>, std::equal_to<shards::OwnedVar>,
                     boost::alignment::aligned_allocator<std::pair<const shards::OwnedVar, SHVar *>, 16>>
      refs;

  boost::container::stable_vector<SHFlow> _flowPool;

  std::vector<std::string> _errors;
  std::vector<SHWire *> _failedWires;
};

namespace shards {
inline SHContext *getRootContext(SHContext *current) {
  while (current->parent) {
    current = current->parent;
  }
  return current;
}

template <typename DELEGATE> auto callOnMeshThread(SHContext *context, DELEGATE &func) -> decltype(func.action(), void()) {
  if (context) {
    if (unlikely(context->onWorkerThread)) {
      throw ActivationError("Trying to callOnMeshThread from a worker thread!");
    }

    // shassert(!context->onLastResume && "Trying to callOnMeshThread from a wire that is about to stop!");
    shassert(context->continuation && "Context has no continuation!");
    shassert(context->currentWire() && "Context has no current wire!");
    shassert(!context->meshThreadTask && "Context already has a mesh thread task!");

    // ok this is the key, we want to go back to the root context and execute there to ensure we are calling from mesh thread
    // indeed and not from any nested coroutine (Step etc)
    auto rootContext = getRootContext(context);

    rootContext->meshThreadTask.connect<&DELEGATE::action>(func);

    // after suspend context might be invalid!
    auto currentWire = context->currentWire();
    SH_CORO_SUSPENDED(currentWire);
    coroutineResume(*rootContext->continuation); // on root context!
    SH_CORO_RESUMED(currentWire);

    shassert(context->currentWire() == currentWire && "Context changed wire during callOnMeshThread!");
    shassert(!rootContext->meshThreadTask && "Context still has a mesh thread task!");
  } else {
    SHLOG_WARNING("NO Context, not running on mesh thread");
    func.action();
  }
}

template <typename L, typename V = std::enable_if_t<std::is_invocable_v<L>>> void callOnMeshThread(SHContext *context, L &&func) {
  struct Action {
    L &lambda;
    void action() { lambda(); }
  } l{func};
  callOnMeshThread(context, l);
}

struct Serialization {
  std::unordered_map<SHVar, SHWireRef> wires;
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
      destroyVar(output);
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

#if 0
      if (output.payload.bytesSize <= 8) {
        // short array
        output.payload.bytesValue = output.shortBytes;
        output.payload.bytesCapacity = 8;
      } else
#endif
      {
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
      }

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
    case SHType::String:
    case SHType::Path:
    case SHType::ContextVar: {
      auto availChars = recycle ? output.payload.stringCapacity : 0;
      read((uint8_t *)&output.payload.stringLen, sizeof(uint32_t));

#if 0
      if (output.payload.stringLen <= 7) {
        // small string, just use the stack
        output.payload.stringValue = output.shortString;
        output.payload.stringCapacity = 7;
      } else
#endif
      {
        if (availChars > 0 && availChars < output.payload.stringLen) {
          // we need more chars then what we have, realloc
          delete[] output.payload.stringValue;
          output.payload.stringValue = new char[output.payload.stringLen + 1];
        } else if (availChars == 0) {
          // just alloc
          output.payload.stringValue = new char[output.payload.stringLen + 1];
        } // else recycling

        // record actualSize
        output.payload.stringCapacity = std::max(availChars, output.payload.stringLen);
      }

      read((uint8_t *)output.payload.stringValue, output.payload.stringLen);
      const_cast<char *>(output.payload.stringValue)[output.payload.stringLen] = 0;
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
          destroyVar(output);
          output.valueType = nextType;
        }
      }

      if (!map) {
        map = new SHMap();
        output.payload.tableValue.api = &GetGlobals().TableInterface;
        output.payload.tableValue.opaque = map;
      }

      uint64_t len;
      read((uint8_t *)&len, sizeof(uint64_t));
      for (uint64_t i = 0; i < len; i++) {
        SHVar keyBuf{};
        deserialize(read, keyBuf);
        auto &dst = (*map)[keyBuf];
        destroyVar(keyBuf); // we don't need the key anymore
        deserialize(read, dst);
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
          destroyVar(output);
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
        destroyVar(dst);
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

      auto pixsize = getPixelSize(output);

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
        int32_t idx;
        read((uint8_t *)&idx, sizeof(int32_t));
        if (idx == -1)
          break;
        SHVar tmp{};
        deserialize(read, tmp);
        blk->setParam(blk, idx, &tmp);
        destroyVar(tmp);
      }
      if (blk->setState) {
        SHVar state{};
        deserialize(read, state);
        blk->setState(blk, &state);
        destroyVar(state);
      }
      // also get line and column
      read((uint8_t *)&blk->line, sizeof(uint32_t));
      read((uint8_t *)&blk->column, sizeof(uint32_t));
      incRef(blk);
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

      SHVar hash{};
      deserialize(read, hash);

      // search if we already have this wire!
      auto cit = wires.find(hash);
      if (cit != wires.end()) {
        SHLOG_TRACE("Skipping deserializing wire: {}", SHWire::sharedFromRef(cit->second)->name);
        output.payload.wireValue = SHWire::addRef(cit->second);
        break;
      }

      auto wire = SHWire::make(&buf[0]);
      output.payload.wireValue = wire->newRef();
      wires.emplace(hash, SHWire::addRef(output.payload.wireValue));
      SHLOG_TRACE("Deserializing wire: {}", wire->name);
      read((uint8_t *)&wire->looped, 1);
      read((uint8_t *)&wire->unsafe, 1);
      read((uint8_t *)&wire->pure, 1);
      // shards len
      read((uint8_t *)&len, sizeof(uint32_t));
      // shards
      for (uint32_t i = 0; i < len; i++) {
        SHVar shardVar{};
        deserialize(read, shardVar);
        ShardPtr shard = shardVar.payload.shardValue;
        if (shardVar.valueType != SHType::ShardRef)
          throw shards::SHException("Expected a shard ref!");
        wire->addShard(shardVar.payload.shardValue);
        // shard's owner is now the wire, remove original reference from deserialize
        decRef(shard);
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
    case SHType::Type:
      if (output.payload.typeValue)
        freeDerivedInfo(*output.payload.typeValue);
      else
        output.payload.typeValue = new SHTypeInfo{};
      deserialize(read, *output.payload.typeValue);
      break;
    default:
      throw shards::SHException("Unknown type during deserialization!");
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
        SHVar k;
        SHVar v;
        while (t.api->tableNext(t, &tit, &k, &v)) {
          total += serialize(k, write);
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
      auto pixsize = getPixelSize(input);
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
        throw shards::SHException(fmt::format("Could not create shard: {}.", name));
      }
      auto params = blk->parameters(blk);
      for (uint32_t i = 0; i < params.len; i++) {
        auto idx = int32_t(i);
        auto dval = model->getParam(model, idx);
        auto pval = blk->getParam(blk, idx);
        if (pval != dval) {
          write((const uint8_t *)&idx, sizeof(int32_t));
          total += serialize(pval, write) + sizeof(int32_t);
        }
      }
      int32_t idx = -1; // end of params
      write((const uint8_t *)&idx, sizeof(int32_t));
      total += sizeof(int32_t);
      // optional state
      if (blk->getState) {
        auto state = blk->getState(blk);
        total += serialize(state, write);
      }
      write((const uint8_t *)&blk->line, sizeof(uint32_t));
      total += sizeof(uint32_t);
      write((const uint8_t *)&blk->column, sizeof(uint32_t));
      total += sizeof(uint32_t);
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

      SHVar hash;
      { // Hash
        hash = shards::hash(input);
        total += serialize(hash, write);
      }

      // stop here if we had it already
      if (wires.count(hash) > 0) {
        SHLOG_TRACE("Skipping serializing wire: {}", wire->name);
        break;
      }

      SHLOG_TRACE("Serializing wire: {}", wire->name);
      wires.emplace(hash, SHWire::addRef(input.payload.wireValue));

      { // Looped & Unsafe
        write((const uint8_t *)&wire->looped, 1);
        total += 1;
        write((const uint8_t *)&wire->unsafe, 1);
        total += 1;
        write((const uint8_t *)&wire->pure, 1);
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
        uint64_t len = 0;
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
    case SHType::Type:
      total += serialize(*input.payload.typeValue, write);
      break;
    default:
      throw shards::SHException("Unknown type during serialization!");
    }
    return total;
  }

  template <class BinaryReader> void deserialize(BinaryReader &read, SHTypeInfo &output) {
    read((uint8_t *)&output.basicType, sizeof(uint8_t));
    switch (output.basicType) {
    case SHType::None:
    case SHType::Any:
    case SHType::Bool:
    case SHType::Int:
    case SHType::Int2:
    case SHType::Int3:
    case SHType::Int4:
    case SHType::Int8:
    case SHType::Int16:
    case SHType::Float:
    case SHType::Float2:
    case SHType::Float3:
    case SHType::Float4:
    case SHType::Color:
    case SHType::Bytes:
    case SHType::String:
    case SHType::Path:
    case SHType::ContextVar:
    case SHType::Image:
    case SHType::Wire:
    case SHType::ShardRef:
    case SHType::Array:
    case SHType::Set:
    case SHType::Audio:
    case SHType::Type:
      // No extra data
      break;
    case SHType::Table: {
      uint32_t len = 0;
      read((uint8_t *)&len, sizeof(uint32_t));
      for (uint32_t i = 0; i < len; i++) {
        SHVar key{};
        deserialize(read, key);
        arrayPush(output.table.keys, key);
      }
      len = 0;
      read((uint8_t *)&len, sizeof(uint32_t));
      for (uint32_t i = 0; i < len; i++) {
        SHTypeInfo info{};
        deserialize(read, info);
        arrayPush(output.table.types, info);
      }
    } break;
    case SHType::Seq: {
      uint32_t len = 0;
      read((uint8_t *)&len, sizeof(uint32_t));
      for (uint32_t i = 0; i < len; i++) {
        SHTypeInfo info{};
        deserialize(read, info);
        arrayPush(output.seqTypes, info);
      }
    } break;
    case SHType::Object:
      read((uint8_t *)&output.object, sizeof(output.object));
      break;
    case SHType::Enum:
      read((uint8_t *)&output.enumeration, sizeof(output.enumeration));
      break;
    default:
      throw shards::SHException("Invalid type to deserialize");
    }
  }

  template <class BinaryWriter> size_t serialize(const SHTypeInfo &input, BinaryWriter &write) {
    size_t total{};
    write((const uint8_t *)&input.basicType, sizeof(uint8_t));
    total += sizeof(uint8_t);
    switch (input.basicType) {
    case SHType::None:
    case SHType::Any:
    case SHType::Bool:
    case SHType::Int:
    case SHType::Int2:
    case SHType::Int3:
    case SHType::Int4:
    case SHType::Int8:
    case SHType::Int16:
    case SHType::Float:
    case SHType::Float2:
    case SHType::Float3:
    case SHType::Float4:
    case SHType::Color:
    case SHType::Bytes:
    case SHType::String:
    case SHType::Path:
    case SHType::ContextVar:
    case SHType::Image:
    case SHType::Wire:
    case SHType::ShardRef:
    case SHType::Array:
    case SHType::Set:
    case SHType::Audio:
    case SHType::Type:
      // No extra data
      break;
    case SHType::Table:
      write((const uint8_t *)&input.table.keys.len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      for (uint32_t i = 0; i < input.table.keys.len; i++) {
        total += serialize(input.table.keys.elements[i], write);
      }
      write((const uint8_t *)&input.table.types.len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      for (uint32_t i = 0; i < input.table.types.len; i++) {
        total += serialize(input.table.types.elements[i], write);
      }
      break;
    case SHType::Seq:
      write((const uint8_t *)&input.seqTypes.len, sizeof(uint32_t));
      total += sizeof(uint32_t);
      for (uint32_t i = 0; i < input.seqTypes.len; i++) {
        total += serialize(input.seqTypes.elements[i], write);
      }
      break;
    case SHType::Object:
      write((const uint8_t *)&input.object, sizeof(input.object));
      total += sizeof(input.object);
      break;
    case SHType::Enum:
      write((const uint8_t *)&input.enumeration, sizeof(input.enumeration));
      total += sizeof(input.enumeration);
      break;
    default:
      throw shards::SHException("Invalid type to serialize");
    }
    return total;
  }
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
} // namespace shards

#endif // SH_CORE_RUNTIME

#ifndef BB0A9620_422B_4277_B3B8_DB67729FA942
#define BB0A9620_422B_4277_B3B8_DB67729FA942

#include "shards.h"
#include "time.hpp"
#include <coroutine>
#include <optional>
#include <vector>
#include <cassert>

namespace shards {
struct coroutine_promise;
struct coroutine : public std::coroutine_handle<coroutine_promise> {
  using promise_type = struct coroutine_promise;
};

struct coroutine_promise {
  coroutine get_return_object() { return {coroutine::from_promise(*this)}; }
  std::suspend_always initial_suspend() noexcept { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  void return_void() {}
  void unhandled_exception() {}
};
SHWireState suspend(SHContext *context, double seconds);
} // namespace shards

struct SHCoro;
struct SHWire;
struct SHFlow;

#define SH_SUSPEND(_ctx_, _secs_)                             \
  const auto _suspend_state = shards::suspend(_ctx_, _secs_); \
  if (_suspend_state != SHWireState::Continue)                \
  return shards::Var::Empty

struct SHContext {
  SHContext(SHCoro *coro, const SHWire *starter, SHFlow *flow) : main(starter), flow(flow), continuation(coro) {
    wireStack.push_back(const_cast<SHWire *>(starter));
  }

  const SHWire *main;
  SHFlow *flow;
  std::vector<SHWire *> wireStack;
  bool onCleanup{false};
  bool onLastResume{false};

  // Used within the coro& stack! (suspend, etc)
  SHCoro *continuation{nullptr};
  SHDuration next{};

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

  constexpr void rebaseFlow() { state = SHWireState::Rebase; }

  constexpr void continueFlow() { state = SHWireState::Continue; }

  constexpr bool shouldContinue() const { return state == SHWireState::Continue; }

  constexpr bool shouldReturn() const { return state == SHWireState::Return; }

  constexpr bool shouldStop() const { return state == SHWireState::Stop; }

  constexpr bool failed() const { return state == SHWireState::Error; }

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
  std::string errorMessage;
  mutable std::vector<const Shard *> nextFrameShards;
};

struct SHCoro {
  shards::coroutine handle;
  std::optional<SHContext> context;

  void init(shards::coroutine &&handle);
  void resume();
  shards::coroutine suspend();
  shards::coroutine yield();
  ~SHCoro();

  // Return true if alive
  operator bool() const;
};

#endif /* BB0A9620_422B_4277_B3B8_DB67729FA942 */

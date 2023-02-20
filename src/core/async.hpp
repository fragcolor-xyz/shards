#ifndef F80CEE03_D5CE_4787_8D65_FB8CC200104A
#define F80CEE03_D5CE_4787_8D65_FB8CC200104A

#include "shards.h"
#include "load_balancing_pool.hpp"
#include <future>

// legacy
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

namespace shards {

#ifdef __EMSCRIPTEN__
// limit to 4 under emscripten
struct SharedThreadPoolConcurrency {
  static int get() { return 4; }
};
#else
struct SharedThreadPoolConcurrency {
  static int get() {
    const auto sys = std::thread::hardware_concurrency();
    return sys > 4 ? sys * 2 : 4;
  }
};
#endif

extern Shared<LoadBalancingPool<SHVar>, SharedThreadPoolConcurrency> SharedThreadPool;
extern Shared<LoadBalancingPool<void>, SharedThreadPoolConcurrency> SharedThreadPoolNR;

template <typename FUNC, typename CANCELLATION>
inline SHVar awaitne(SHContext *context, FUNC &&func, CANCELLATION &&cancel) noexcept {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  return func();
#else
  SHVar res{};
  auto future = shards::SharedThreadPool().submit(func);
  auto complete = future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
  while (!complete && context->shouldContinue()) {
    if (shards::suspend(context, 0) != SHWireState::Continue)
      break;
    complete = future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
  }

  if (!complete) {
    cancel();
    future.wait(); // hmm, this might block forever
  }

  try {
    res = future.get();
  } catch (const std::exception &e) {
    context->cancelFlow(e.what());
  } catch (...) {
    context->cancelFlow("foreign exception failure");
  }

  return res;
#endif
}

template <typename FUNC, typename CANCELLATION> inline void await(SHContext *context, FUNC &&func, CANCELLATION &&cancel) {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  func();
#else
  auto future = shards::SharedThreadPoolNR().submit(func);
  auto complete = future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
  while (!complete && context->shouldContinue()) {
    if (shards::suspend(context, 0) != SHWireState::Continue)
      break;
    complete = future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
  }

  if (!complete) {
    cancel();
    future.wait(); // hmm, this might block forever
  }

  try {
    future.get();
  } catch (const std::exception &e) {
    context->cancelFlow(e.what());
  } catch (...) {
    context->cancelFlow("foreign exception failure");
  }
#endif
}
} // namespace shards

#endif /* F80CEE03_D5CE_4787_8D65_FB8CC200104A */

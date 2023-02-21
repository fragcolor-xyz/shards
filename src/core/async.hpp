#ifndef F80CEE03_D5CE_4787_8D65_FB8CC200104A
#define F80CEE03_D5CE_4787_8D65_FB8CC200104A

#include "shards.h"
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
extern Shared<boost::asio::thread_pool, SharedThreadPoolConcurrency> SharedThreadPool;

template <typename FUNC, typename CANCELLATION>
inline SHVar awaitne(SHContext *context, FUNC &&func, CANCELLATION &&cancel) noexcept {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  return func();
#else
  SHVar res{};
  std::packaged_task<SHVar()> task(std::move(func));
  auto future = task.get_future();

  boost::asio::post(shards::SharedThreadPool(), std::move(task));

  auto complete = future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
  while (!complete && context->shouldContinue()) {
    if (shards::suspend(context, 0) != SHWireState::Continue)
      break;
    complete = future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
  }

  if (unlikely(!complete)) {
    cancel();
    SHLOG_DEBUG("awaitne: canceling task");
    auto state = future.wait_for(std::chrono::milliseconds(5000)); // give it 5 seconds to complete? TBD, should not happen
    if (state != std::future_status::ready) {
      context->cancelFlow("awaitne: task canceled but potentially still running");
      return res;
    }
    SHLOG_DEBUG("awaitne: task canceled");
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
  std::packaged_task<void()> task(std::move(func));
  auto future = task.get_future();

  boost::asio::post(shards::SharedThreadPool(), std::move(task));

  auto complete = future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
  while (!complete && context->shouldContinue()) {
    if (shards::suspend(context, 0) != SHWireState::Continue)
      break;
    complete = future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
  }

  if (unlikely(!complete)) {
    cancel();
    SHLOG_DEBUG("await: canceling task");
    auto state = future.wait_for(std::chrono::milliseconds(5000)); // give it 5 seconds to complete? TBD, should not happen
    if (state != std::future_status::ready)
      throw std::runtime_error("await: task canceled but potentially still running");
    SHLOG_DEBUG("await: task canceled");
  }

  future.get(); // can throw!
#endif
}
} // namespace shards

#endif /* F80CEE03_D5CE_4787_8D65_FB8CC200104A */

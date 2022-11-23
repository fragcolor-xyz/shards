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
  std::exception_ptr exp = nullptr;
  SHVar res{};
  std::atomic_bool complete = false;

  boost::asio::post(shards::SharedThreadPool(), [&]() {
    try {
      // SHLOG_TRACE("Awaitne: starting {}", reinterpret_cast<void *>(&func));
      res = func();
    } catch (...) {
      exp = std::current_exception();
    }
    complete = true;
  });

  // SHLOG_TRACE("Awaitne: waiting for completion {}", reinterpret_cast<void *>(&func));

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

  boost::asio::post(shards::SharedThreadPool(), [&]() {
    try {
      // SHLOG_TRACE("Await: starting {}", reinterpret_cast<void *>(&func));
      func();
    } catch (...) {
      exp = std::current_exception();
    }
    complete = true;
  });

  // SHLOG_TRACE("Await: waiting for completion {}", reinterpret_cast<void *>(&func));

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

#endif /* F80CEE03_D5CE_4787_8D65_FB8CC200104A */

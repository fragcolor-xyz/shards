#ifndef F80CEE03_D5CE_4787_8D65_FB8CC200104A
#define F80CEE03_D5CE_4787_8D65_FB8CC200104A

#include "foundation.hpp"
#include "gfx/feature.hpp"
#include "runtime.hpp"
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/bind/bind.hpp>

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
    return sys > 4 ? sys : 4;
  }
};
#endif

extern Shared<boost::asio::thread_pool, SharedThreadPoolConcurrency> SharedThreadPool;

template <typename FUNC, typename CANCELLATION>
inline SHVar awaitne(SHContext *context, FUNC &&func, CANCELLATION &&cancel) noexcept {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  return func();
#else
  struct BlockingCall {
    FUNC &&func;

    std::exception_ptr exp;
    SHVar res;
    std::atomic_bool complete;

    void call() {
      try {
        res = func();
      } catch (...) {
        exp = std::current_exception();
      }
      complete = true;
    }
  } call{std::forward<FUNC>(func)};

  boost::asio::post(shards::SharedThreadPool(), boost::bind(&BlockingCall::call, &call));

  while (!call.complete && context->shouldContinue()) {
    if (shards::suspend(context, 0) != SHWireState::Continue)
      break;
  }

  if (unlikely(!call.complete)) {
    cancel();
    while (!call.complete) {
      std::this_thread::yield();
    }
  }

  if (call.exp) {
    try {
      std::rethrow_exception(call.exp);
    } catch (const std::exception &e) {
      context->cancelFlow(e.what());
    } catch (...) {
      context->cancelFlow("foreign exception failure");
    }
  }

  return call.res;
#endif
}

template <typename FUNC, typename CANCELLATION> inline void await(SHContext *context, FUNC &&func, CANCELLATION &&cancel) {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  func();
#else
  struct BlockingCall {
    FUNC &&func;

    std::exception_ptr exp;
    std::atomic_bool complete;

    void call() {
      try {
        func();
      } catch (...) {
        exp = std::current_exception();
      }
      complete = true;
    }
  } call{std::forward<FUNC>(func)};

  boost::asio::post(shards::SharedThreadPool(), boost::bind(&BlockingCall::call, &call));

  while (!call.complete && context->shouldContinue()) {
    if (shards::suspend(context, 0) != SHWireState::Continue)
      break;
  }

  if (unlikely(!call.complete)) {
    cancel();
    while (!call.complete) {
      std::this_thread::yield();
    }
  }

  if (call.exp) {
    std::rethrow_exception(call.exp);
  }
#endif
}
} // namespace shards

#endif /* F80CEE03_D5CE_4787_8D65_FB8CC200104A */

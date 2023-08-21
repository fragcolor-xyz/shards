#ifndef F80CEE03_D5CE_4787_8D65_FB8CC200104A
#define F80CEE03_D5CE_4787_8D65_FB8CC200104A

#include <chrono>
#include <thread>
#include "foundation.hpp"
#include "runtime.hpp"
#include <boost/lockfree/queue.hpp>

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#define HAS_ASYNC_SUPPORT 0
#else
#define HAS_ASYNC_SUPPORT 1
#endif

namespace shards {
#if HAS_ASYNC_SUPPORT

/*
 * The TidePool class is a simple C++ thread pool implementation designed to manage
 * worker threads and distribute tasks to them. The class supports dynamic adjustment
 * of the number of worker threads based on the current workload.
 *
 * Features:
 * - Abstract Work struct representing tasks to be executed by the worker threads.
 * - Dynamic adjustment of the number of worker threads based on the number of tasks in the queue.
 * - Lock-free queue for efficient task scheduling.
 * - Configurable minimum, initial, and maximum number of worker threads.
 * - Asynchronous controller thread that manages worker threads.
 * - Simple scheduling function for adding tasks to the queue.
 *
 * Usage:
 * - Derive custom work classes from the Work struct and implement the call() function.
 * - Create an instance of TidePool.
 * - Schedule tasks using the schedule() function.
 * - The pool will automatically adjust the number of worker threads based on the workload.
 */

struct TidePool {
  struct Work {
    virtual void call() = 0;
  };

  struct Worker {
    Worker(boost::lockfree::queue<Work *> &queue, std::atomic_size_t &counter) : _queue(queue), _counter(counter) {
      _running = true;
      _future = std::async(std::launch::async, [this]() {
        while (_running) {
          Work *work{};
          if (_queue.pop(work)) {
            // SHLOG_DEBUG("TidePool: calling {}", (void*)work);
            work->call();
            _counter--;
          } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
        }
      });
    }

    std::future<void> _future;
    std::atomic_bool _running;
    boost::lockfree::queue<Work *> &_queue;
    std::atomic_size_t &_counter;
  };

  static constexpr size_t LowWater = 4;
  static constexpr size_t NumWorkers = 8;
  static constexpr size_t MaxWorkers = 32;

  std::atomic_size_t _scheduledCounter;
  std::atomic_bool _running;
  std::future<void> _controller;
  boost::lockfree::queue<Work *> _queue{NumWorkers};
  std::deque<Worker> _workers;

  TidePool() {
    _running = true;
    _controller = std::async(std::launch::async, [this]() { controllerWorker(); });
  }

  ~TidePool() {
    _running = false;
    _controller.wait();
  }

  void schedule(Work *work) {
    _scheduledCounter++;
    _queue.push(work);
  }

  void controllerWorker() {
    // spawn workers first
    for (size_t i = 0; i < NumWorkers; ++i) {
      _workers.emplace_back(_queue, _scheduledCounter);
    }

    while (_running) {
      assert(_workers.size() >= NumWorkers);

      if (_scheduledCounter < LowWater && _workers.size() > NumWorkers) {
        // we have less than LowWater scheduled and we have more than NumWorkers workers
        _workers.back()._running = false;
        _workers.pop_back();
        // SHLOG_DEBUG("TidePool: worker removed, count: {}", _workers.size());
      } else if (_scheduledCounter > _workers.size() && _workers.size() < MaxWorkers) {
        // we have more scheduled than workers
        _workers.emplace_back(_queue, _scheduledCounter);
        // SHLOG_DEBUG("TidePool: worker added, count: {}", _workers.size());
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (auto &worker : _workers) {
      worker._running = false;
      worker._future.wait();
    }
  }
};

TidePool &getTidePool();
#endif

template <typename FUNC, typename CANCELLATION>
inline SHVar awaitne(SHContext *context, FUNC &&func, CANCELLATION &&cancel) noexcept {
  static_assert(std::is_same_v<decltype(func()), SHVar> || std::is_same_v<decltype(func()), Var>,
                "func must return SHVar or Var");

#if !HAS_ASYNC_SUPPORT
  return func();
#else
  struct BlockingCall : TidePool::Work {
    BlockingCall(FUNC &&func) : func(std::move(func)), exp(), res(), complete(false) {}

    FUNC &&func;

    std::exception_ptr exp;
    SHVar res;
    std::atomic_bool complete;

    virtual void call() {
      try {
        res = func();
      } catch (...) {
        exp = std::current_exception();
      }
      complete = true;
    }
  } call{std::forward<FUNC>(func)};

  getTidePool().schedule(&call);

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
#if !HAS_ASYNC_SUPPORT
  func();
#else
  struct BlockingCall : TidePool::Work {
    BlockingCall(FUNC &&func) : func(std::move(func)), exp(), complete(false) {}

    FUNC &&func;

    std::exception_ptr exp;
    std::atomic_bool complete;

    virtual void call() {
      try {
        func();
      } catch (...) {
        exp = std::current_exception();
      }
      complete = true;
    }
  } call{std::forward<FUNC>(func)};

  getTidePool().schedule(&call);

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

/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include "foundation.hpp"
#include "coro.hpp"
#include "utils.hpp"
#include <memory>

#if SH_DEBUG_CONSISTENT_RESUMER
#include <SDL3/SDL_stdinc.h>
#endif

// Enable for verbose fiber logging
#ifndef SH_EM_FIBER_TRACE_LOGS
#define SH_EM_FIBER_TRACE_LOGS 0
#endif

#if SH_EM_FIBER_TRACE_LOGS
#define SH_FIBER_TRACE_LOG(...) SHLOG_TRACE(__VA_ARGS__)
#else
#define SH_FIBER_TRACE_LOG(...)
#endif

namespace shards {
#if SH_USE_THREAD_FIBER
#define SH_DEBUG_THREAD_STACK_SIZE 2 * 1024 * 1024
ThreadFiber::~ThreadFiber() {}

void ThreadFiber::init(std::function<void()> fn) {
  isRunning = true;

  // Initial suspend
  boost::thread::attributes attrs;
  attrs.set_stack_size(SH_DEBUG_THREAD_STACK_SIZE);
  thread.emplace(attrs, [=]() {
    try {
      fn();
    } catch (std::exception &e) {
      SHLOG_ERROR("ThreadFiber unhandled exception: {}", e.what());
    }

    // Final suspend
    finished = true;
    switchToCaller();
  });

  // Wait for initial suspend
  std::unique_lock<decltype(mtx)> l(mtx);
  while (isRunning) {
    cv.wait(l);
  }
}

void ThreadFiber::resume() {
  if (isRunning) {
    switchToCaller();
  } else {
    // SPDLOG_TRACE("CALLER> {}", thread->get_id());
    switchToThread();
    // SPDLOG_TRACE("CALLER< {}", thread->get_id());
    if (finished) {
      thread->join();
      thread.reset();
    }
  }
}

void ThreadFiber::suspend() {
  // SPDLOG_TRACE("RUNNER< {}", thread->get_id());
  shassert(isRunning && "Cannot suspend an already suspended fiber");
  switchToCaller();
  // SPDLOG_TRACE("RUNNER> {}", thread->get_id());
}

void ThreadFiber::switchToCaller() {
  std::unique_lock<decltype(mtx)> l(mtx);
  isRunning = false;
  cv.notify_all();
  if (!finished) {
    while (!isRunning) {
      cv.wait(l);
    }
  }
}
void ThreadFiber::switchToThread() {
  std::unique_lock<decltype(mtx)> l(mtx);
  isRunning = true;
  cv.notify_all();
  while (isRunning) {
    cv.wait(l);
  }
}

ThreadFiber::operator bool() const { return !finished; }

#else // SH_USE_THREAD_FIBER
#ifndef __EMSCRIPTEN__

#if SH_DEBUG_CONSISTENT_RESUMER
static bool checkForConsistentResumer() {
  static bool check = []() {
    if (SDL_getenv("SH_IGNORE_CONSISTENT_RESUMER"))
      return false;
    return true;
  }();
  return check;
}
#endif

Fiber::Fiber(SHStackAllocator allocator) : allocator(allocator) {}
void Fiber::init(std::function<void()> fn) {
#if SH_DEBUG_CONSISTENT_RESUMER
  consistentResumer.emplace(std::this_thread::get_id());
#if SH_DEBUG_CONSISTENT_RESUMER > 1
  creatorStack = getThreadNameStack();
#endif
#endif
  continuation.emplace(boost::context::callcc(std::allocator_arg, allocator, [=](boost::context::continuation &&sink) {
    continuation.emplace(std::move(sink));
    fn();
    return std::move(continuation.value());
  }));
}
void Fiber::resume() {
  shassert(continuation);
#if SH_DEBUG_CONSISTENT_RESUMER
  if (checkForConsistentResumer() && (!consistentResumer || *consistentResumer != std::this_thread::get_id()))
    throw std::runtime_error("Fiber::resume() called from different thread");
#endif
  continuation = continuation->resume();
}
void Fiber::suspend() {
  shassert(continuation);
#if SH_DEBUG_CONSISTENT_RESUMER
  if (checkForConsistentResumer() && (!consistentResumer || *consistentResumer != std::this_thread::get_id()))
    throw std::runtime_error("Fiber::suspend() called from different thread");
#endif
  continuation = continuation->resume();
}
Fiber::operator bool() const { return continuation.has_value() && (bool)continuation.value(); }

#else // __EMSCRIPTEN__

thread_local emscripten_fiber_t *em_local_coro{nullptr};
thread_local emscripten_fiber_t em_main_coro{};
thread_local uint8_t em_asyncify_main_stack[Fiber::as_stack_size];

[[noreturn]] static void action(void *p) {
  SH_FIBER_TRACE_LOG("EM FIBER ACTION RUN");
  auto coro = reinterpret_cast<Fiber *>(p);
  coro->func();
  // If entry_func returns, the entire program will end, as if main had
  // returned.
  abort();
}

void Fiber::init(const std::function<void()> &func) {
  SH_FIBER_TRACE_LOG("EM FIBER INIT");
  this->func = func;
  c_stack = new (std::align_val_t{16}) uint8_t[stack_size];
  emscripten_fiber_init(&em_fiber, action, this, c_stack, stack_size, asyncify_stack, as_stack_size);

  // Initial resume to match functionality of other Fiber implementations
  resume();
}

NO_INLINE void Fiber::resume() {
  SH_FIBER_TRACE_LOG("EM FIBER SWAP RESUME {}", reinterpret_cast<uintptr_t>(&em_fiber));
  // ensure local thread is setup
  if (!em_main_coro.stack_ptr) {
    SHLOG_DEBUG("Fiber - initialization of new thread");
    emscripten_fiber_init_from_current_context(&em_main_coro, em_asyncify_main_stack, Fiber::as_stack_size);
  }
  // ensure we have a local coro
  if (!em_local_coro) {
    em_local_coro = &em_main_coro;
  }
  // from current to new
  em_parent_fiber = em_local_coro;
  em_local_coro = &em_fiber;
  emscripten_fiber_swap(em_parent_fiber, &em_fiber);
}

NO_INLINE void Fiber::suspend() {
  SH_FIBER_TRACE_LOG("EM FIBER SWAP SUSPEND {}", reinterpret_cast<uintptr_t>(&em_fiber));
  // always yields to main
  shassert(em_parent_fiber);
  em_local_coro = em_parent_fiber;
  emscripten_fiber_swap(&em_fiber, em_parent_fiber);
}

#endif
#endif
} // namespace shards

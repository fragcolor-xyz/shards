/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include "foundation.hpp"
#include "coro.hpp"
#include "utils.hpp"
#include <memory>

#if SH_DEBUG_CONSISTENT_RESUMER
#include <cstdlib>
#endif

#if defined(BOOST_USE_VALGRIND) || defined(SHARDS_VALGRIND)
#include <valgrind/valgrind.h>
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
  thread.emplace(attrs, [this, fn]() {
    logging::LogContext lctx;
    logContext = &lctx;
    DEFER({ logContext = nullptr; });
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
  if (logContext) {
    logContext->unlink();
  }

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
  srcLogContext = logging::ThreadState::get().current;
  if (srcLogContext) {
    logContext->linkRootTo(srcLogContext);
  }

  std::unique_lock<decltype(mtx)> l(mtx);
  isRunning = true;
  cv.notify_all();
  while (isRunning) {
    cv.wait(l);
  }
}

ThreadFiber::operator bool() const { return !finished; }

#elif SH_CUSTOM_FCONTEXT
// Custom fcontext-based fiber implementation

#if SH_DEBUG_CONSISTENT_RESUMER
static bool checkForConsistentResumerCustom() {
  static bool check = []() {
    if (std::getenv("SH_IGNORE_CONSISTENT_RESUMER"))
      return false;
    return true;
  }();
  return check;
}
#endif

#ifdef SH_USE_TSAN
// Thread-local storage for the main TSAN fiber handle
static thread_local void *tsan_main_fiber_custom = nullptr;

static ALWAYS_INLINE void *getTsanMainFiberCustom() {
  if (!tsan_main_fiber_custom) {
    tsan_main_fiber_custom = __tsan_get_current_fiber();
  }
  return tsan_main_fiber_custom;
}
#endif

Fiber::Fiber(SHStackAllocator allocator) : allocator(allocator) {}

Fiber::~Fiber() {
#if defined(BOOST_USE_VALGRIND) || defined(SHARDS_VALGRIND)
  if (valgrind_stack_id) {
    VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
  }
#endif

#ifdef SH_USE_TSAN
  if (tsan_fiber) {
    void *current_fiber = __tsan_get_current_fiber();
    if (current_fiber == tsan_fiber) {
      __tsan_switch_to_fiber(getTsanMainFiberCustom(), 0);
    }
    __tsan_destroy_fiber(tsan_fiber);
    tsan_fiber = nullptr;
  }
#endif
}

void Fiber::fcontextEntry(fcontext::transfer_t t) {
  // The transfer contains the Fiber pointer and the caller's context
  Fiber *self = static_cast<Fiber *>(t.data);

  // Store the caller's context so we can return to it
  self->ctx = t.fctx;

#ifdef SH_USE_ASAN
  // We just switched TO this fiber, finish the switch
  __sanitizer_finish_switch_fiber(self->asan_init_fake_stack, nullptr, nullptr);
#endif

  // Run the user's function
  self->func();

  // Function returned - switch back to caller
#ifdef SH_USE_ASAN
  __sanitizer_start_switch_fiber(nullptr, nullptr, 0);
#endif

#ifdef SH_USE_TSAN
  // Switch TSAN tracking to main fiber before final jump.
  // The fiber will be destroyed after resume() returns, and the
  // destructor properly checks and handles the TSAN state.
  __tsan_switch_to_fiber(getTsanMainFiberCustom(), 0);
#endif

  // Jump back to caller - after this the fiber should not be resumed.
  // Note: ctx remains valid after this; the caller should not resume
  // the fiber once the function has returned.
  fcontext::sh_jump_fcontext(self->ctx, nullptr);
}

void Fiber::init(std::function<void()> fn) {
  // Validate allocator before use
  shassert(allocator.mem != nullptr && "Stack memory is null");
  shassert(allocator.size >= 4096 && "Stack size too small (minimum 4096)");
  // Prevent double-init
  shassert(!ctx && "Fiber::init() called twice");

#if SH_DEBUG_CONSISTENT_RESUMER
  consistentResumer.emplace(std::this_thread::get_id());
#endif

  func = std::move(fn);

#if defined(BOOST_USE_VALGRIND) || defined(SHARDS_VALGRIND)
  // Register stack with Valgrind for proper stack tracking
  void *sp = allocator.mem + allocator.size;
  valgrind_stack_id = VALGRIND_STACK_REGISTER(sp, allocator.mem);
#endif

#ifdef SH_USE_ASAN
  asan_stack_bottom = allocator.mem;
  asan_stack_size = allocator.size;
  __asan_unpoison_memory_region(asan_stack_bottom, asan_stack_size);
#endif

#ifdef SH_USE_TSAN
  getTsanMainFiberCustom();
  shassert(!tsan_fiber && "Fiber::init() called twice");
  tsan_fiber = __tsan_create_fiber(0);
#endif

  // Create the context - sp points to top of stack (base + size)
  void *sp = allocator.mem + allocator.size;
  ctx = fcontext::sh_make_fcontext(sp, allocator.mem, &Fiber::fcontextEntry);

#ifdef SH_USE_ASAN
  void *init_fake_stack = nullptr;
  __sanitizer_start_switch_fiber(&init_fake_stack, asan_stack_bottom, asan_stack_size);
  asan_init_fake_stack = init_fake_stack;
#endif

#ifdef SH_USE_TSAN
  __tsan_switch_to_fiber(tsan_fiber, 0);
#endif

  // Do initial resume to run until first suspend
  fcontext::transfer_t t = fcontext::sh_jump_fcontext(ctx, this);
  ctx = t.fctx;

#ifdef SH_USE_ASAN
  __sanitizer_finish_switch_fiber(init_fake_stack, nullptr, nullptr);
#endif

#ifdef SH_USE_TSAN
  __tsan_switch_to_fiber(getTsanMainFiberCustom(), 0);
#endif
}

void Fiber::resume() {
  shassert(ctx);
#if SH_DEBUG_CONSISTENT_RESUMER
  if (checkForConsistentResumerCustom() && (!consistentResumer || *consistentResumer != std::this_thread::get_id()))
    throw std::runtime_error("Fiber::resume() called from different thread");
#endif

#ifdef SH_USE_ASAN
  void *fake_stack = nullptr;
  __sanitizer_start_switch_fiber(&fake_stack, asan_stack_bottom, asan_stack_size);
#endif

#ifdef SH_USE_TSAN
  __tsan_switch_to_fiber(tsan_fiber, 0);
#endif

  fcontext::transfer_t t = fcontext::sh_jump_fcontext(ctx, this);
  ctx = t.fctx;

#ifdef SH_USE_ASAN
  __sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#endif

#ifdef SH_USE_TSAN
  __tsan_switch_to_fiber(getTsanMainFiberCustom(), 0);
#endif
}

void Fiber::suspend() {
  shassert(ctx);
#if SH_DEBUG_CONSISTENT_RESUMER
  if (checkForConsistentResumerCustom() && (!consistentResumer || *consistentResumer != std::this_thread::get_id()))
    throw std::runtime_error("Fiber::suspend() called from different thread");
#endif

#ifdef SH_USE_ASAN
  void *fake_stack = nullptr;
  __sanitizer_start_switch_fiber(&fake_stack, nullptr, 0);
#endif

#ifdef SH_USE_TSAN
  __tsan_switch_to_fiber(getTsanMainFiberCustom(), 0);
#endif

  fcontext::transfer_t t = fcontext::sh_jump_fcontext(ctx, nullptr);
  ctx = t.fctx;

#ifdef SH_USE_ASAN
  __sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#endif
}

Fiber::operator bool() const {
  return ctx != nullptr;
}

#elif !defined(__EMSCRIPTEN__)
// Boost.Context based fiber implementation (fallback)

#if SH_DEBUG_CONSISTENT_RESUMER
static bool checkForConsistentResumer() {
  static bool check = []() {
    if (std::getenv("SH_IGNORE_CONSISTENT_RESUMER"))
      return false;
    return true;
  }();
  return check;
}
#endif

#ifdef SH_USE_TSAN
// Thread-local storage for the main TSAN fiber handle (the fiber that resumes coroutines)
static thread_local void *tsan_main_fiber = nullptr;

static ALWAYS_INLINE void *getTsanMainFiber() {
  if (!tsan_main_fiber) {
    tsan_main_fiber = __tsan_get_current_fiber();
  }
  return tsan_main_fiber;
}
#endif

Fiber::Fiber(SHStackAllocator allocator) : allocator(allocator) {}

Fiber::~Fiber() {
#ifdef SH_USE_TSAN
  if (tsan_fiber) {
    // Ensure we're not on this fiber's context before destroying
    // (handles edge case where fiber function returned instead of suspending)
    void *current_fiber = __tsan_get_current_fiber();
    if (current_fiber == tsan_fiber) {
      __tsan_switch_to_fiber(getTsanMainFiber(), 0);
    }
    __tsan_destroy_fiber(tsan_fiber);
    tsan_fiber = nullptr;
  }
#endif
}
void Fiber::init(std::function<void()> fn) {
#if SH_DEBUG_CONSISTENT_RESUMER
  consistentResumer.emplace(std::this_thread::get_id());
#if SH_DEBUG_CONSISTENT_RESUMER > 1
  creatorStack = getThreadNameStack();
#endif
#endif

#ifdef SH_USE_ASAN
  // Store stack information for ASAN
  // Stack bottom is the base of the allocated memory (lowest address)
  // Stack grows downward, so sp (stack pointer) points to the top (highest address)
  asan_stack_bottom = allocator.mem;
  asan_stack_size = allocator.size;

  // Unpoison the entire fiber stack to support exception unwinding
  // Without this, ASAN reports "stack-use-after-return" when exceptions
  // are thrown because the unwinder accesses stack frames ASAN thinks are invalid
  __asan_unpoison_memory_region(asan_stack_bottom, asan_stack_size);
#endif

#ifdef SH_USE_TSAN
  // Ensure main fiber is captured before we create child fibers
  getTsanMainFiber();

  // Create TSAN fiber handle for this coroutine
  // init() should only be called once per Fiber object
  shassert(!tsan_fiber && "Fiber::init() called twice - this is not supported");
  tsan_fiber = __tsan_create_fiber(0);
#endif

#ifdef SH_USE_ASAN
  // Use a local for the initial switch, then store in member for the lambda
  void *init_fake_stack = nullptr;

  // Tell ASAN we're about to switch to the fiber's stack
  __sanitizer_start_switch_fiber(&init_fake_stack, asan_stack_bottom, asan_stack_size);

  // Store in member so lambda can access it safely (local would be dangling after init() returns)
  asan_init_fake_stack = init_fake_stack;
#endif

#ifdef SH_USE_TSAN
  // Switch TSAN tracking to this fiber
  __tsan_switch_to_fiber(tsan_fiber, 0);
#endif

  continuation.emplace(boost::context::callcc(std::allocator_arg, allocator, [this, fn](boost::context::continuation &&sink) {
    continuation.emplace(std::move(sink));

#ifdef SH_USE_ASAN
    // We just switched TO this fiber, finish the switch (use member, not captured local)
    __sanitizer_finish_switch_fiber(asan_init_fake_stack, nullptr, nullptr);
#endif

    fn();

    // The fiber function should never return - it should always suspend
    // But if it does, we need to clean up sanitizer state
#ifdef SH_USE_ASAN
    // Before returning, start switch back to caller (nullptr = main stack)
    __sanitizer_start_switch_fiber(nullptr, nullptr, 0);
#endif
    // Note: TSAN switch is NOT done here - it's handled after callcc returns (line 229)
    // to avoid duplicate switches when fiber returns vs suspends

    return std::move(continuation.value());
  }));

#ifdef SH_USE_ASAN
  // After callcc returns (fiber yielded back to us), finish the switch back to main stack
  __sanitizer_finish_switch_fiber(init_fake_stack, nullptr, nullptr);
#endif

#ifdef SH_USE_TSAN
  // Switch TSAN tracking back to main fiber after the initial yield
  __tsan_switch_to_fiber(getTsanMainFiber(), 0);
#endif
}
void Fiber::resume() {
  shassert(continuation);
#if SH_DEBUG_CONSISTENT_RESUMER
  if (checkForConsistentResumer() && (!consistentResumer || *consistentResumer != std::this_thread::get_id()))
    throw std::runtime_error("Fiber::resume() called from different thread");
#endif

#ifdef SH_USE_ASAN
  // Each context switch needs its own fake_stack (local variable on caller's stack)
  void *fake_stack = nullptr;
  // Tell ASAN we're about to switch to the fiber's stack
  __sanitizer_start_switch_fiber(&fake_stack, asan_stack_bottom, asan_stack_size);
#endif

#ifdef SH_USE_TSAN
  // Switch TSAN tracking to this fiber
  __tsan_switch_to_fiber(tsan_fiber, 0);
#endif

  continuation = continuation->resume();

#ifdef SH_USE_ASAN
  // Fiber yielded back to us, finish the switch back to main stack
  __sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#endif

#ifdef SH_USE_TSAN
  // Switch TSAN tracking back to main fiber after fiber yields
  __tsan_switch_to_fiber(getTsanMainFiber(), 0);
#endif
}
void Fiber::suspend() {
  shassert(continuation);
#if SH_DEBUG_CONSISTENT_RESUMER
  if (checkForConsistentResumer() && (!consistentResumer || *consistentResumer != std::this_thread::get_id()))
    throw std::runtime_error("Fiber::suspend() called from different thread");
#endif

#ifdef SH_USE_ASAN
  // Each context switch needs its own fake_stack (local variable on fiber's stack)
  void *fake_stack = nullptr;
  // Tell ASAN we're about to switch back to the main stack (nullptr = main stack)
  __sanitizer_start_switch_fiber(&fake_stack, nullptr, 0);
#endif

#ifdef SH_USE_TSAN
  // Switch TSAN tracking back to the main fiber
  __tsan_switch_to_fiber(getTsanMainFiber(), 0);
#endif

  continuation = continuation->resume();

#ifdef SH_USE_ASAN
  // We're back on the fiber's stack, finish the switch
  __sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#endif
}
Fiber::operator bool() const { return continuation.has_value() && (bool)continuation.value(); }

#elif defined(__EMSCRIPTEN__) && defined(SHARDS_USE_JSPI)
// JSPI-based fiber implementation (Emscripten without Asyncify)

// Static entry function that JS can call via WebAssembly.promising
static void jspiEntryAction(void *p) {
  SH_FIBER_TRACE_LOG("JSPI FIBER ACTION RUN");
  auto fiber = reinterpret_cast<Fiber *>(p);

  // Notify JS that we're entering this fiber
  shardsFiberEnter(fiber->fiberId);

  try {
    fiber->func();
  } catch (std::exception &e) {
    SHLOG_ERROR("JSPI fiber unhandled exception: {}", e.what());
  } catch (...) {
    SHLOG_ERROR("JSPI fiber unknown exception");
  }

  // Notify JS that fiber is exiting
  shardsFiberExit(fiber->fiberId);

  // Fiber should not return normally - the wire runner handles this
  // by calling coroutineSuspend at the end
}

void Fiber::init(const std::function<void()> &func) {
  SH_FIBER_TRACE_LOG("JSPI FIBER INIT");
  this->func = func;

  // Create the fiber context in JS
  this->fiberId = shardsFiberCreate();

  // Start the fiber - it will run until first suspension
  shardsFiberStartEntry(fiberId, jspiEntryAction, this);
}

NO_INLINE void Fiber::resume() {
  SH_FIBER_TRACE_LOG("JSPI FIBER RESUME id={}", fiberId);
  shardsFiberResume(fiberId);
}

NO_INLINE void Fiber::suspend() {
  SH_FIBER_TRACE_LOG("JSPI FIBER SUSPEND id={}", fiberId);
  shardsFiberSuspend(fiberId);
}

#else // __EMSCRIPTEN__ with Asyncify (default)

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

#endif // SH_USE_THREAD_FIBER / SH_CUSTOM_FCONTEXT / __EMSCRIPTEN__ chain
} // namespace shards

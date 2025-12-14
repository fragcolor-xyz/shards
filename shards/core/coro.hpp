#ifndef D2E9D440_0C35_4166_9CF4_0902B462A99C
#define D2E9D440_0C35_4166_9CF4_0902B462A99C
#include <optional>
#include <cassert>
#include <variant>
#include <functional>
#include <thread>
#include "platform.hpp"

#ifdef SHARDS_DEBUGGER
#define SH_BASE_STACK_SIZE 2 * 1024 * 1024
#endif

#ifndef SH_BASE_STACK_SIZE
#if SH_EMSCRIPTEN
#define SH_BASE_STACK_SIZE 2 * 1024 * 1024
#else
#ifndef NDEBUG
#define SH_BASE_STACK_SIZE 1024 * 1024
#else
#define SH_BASE_STACK_SIZE 128 * 1024
#endif
#endif // SH_EMSCRIPTEN
#endif

// Enable to assert on consistent resuming
// this is required to pass for the emscripten version to work correctly
// since fiber state is stored on the calling JS stack
#ifndef SH_DEBUG_CONSISTENT_RESUMER
#define SH_DEBUG_CONSISTENT_RESUMER 0
#endif

// Enable to assert on consistent resuming
// this is required to pass for the emscripten version to work correctly
// since fiber state is stored on the calling JS stack
#ifndef SH_DEBUG_CONSISTENT_RESUMER
#define SH_DEBUG_CONSISTENT_RESUMER 0
#endif

// Defining SH_USE_THREAD_FIBER uses threads as fibers to aid in debugging
// Set SHARDS_THREAD_FIBER=ON in cmake to enable
#if SH_USE_THREAD_FIBER
#if SH_DEBUG_CONSISTENT_RESUMER
#error "SH_DEBUG_CONSISTENT_RESUMER is not supported with SH_USE_THREAD_FIBER"
#endif
#include <boost/context/continuation_fcontext.hpp>
#include <boost/thread.hpp>
#include <shards/log/log.hpp>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
namespace shards {
// Dedicated-thread coroutine
struct ThreadFiber {
private:
  std::mutex mtx;
  std::condition_variable cv;

  std::atomic_bool finished;
  std::atomic_bool isRunning;

  std::optional<boost::thread> thread;
  logging::LogContext *srcLogContext{};
  logging::LogContext *logContext;

public:
  ThreadFiber() = default;
  ThreadFiber(const ThreadFiber &) = delete;
  ThreadFiber &operator=(const ThreadFiber &) = delete;
  ~ThreadFiber();
  void init(std::function<void()> fn);
  void resume();
  void suspend();
  operator bool() const;

private:
  void switchToCaller();
  void switchToThread();
};
using Fiber = ThreadFiber;
} // namespace shards
#else // SH_USE_THREAD_FIBER
#ifndef __EMSCRIPTEN__
#define SH_CORO_NEED_STACK_MEM 1
#define SH_BOOST_COROUTINE 1
#include <boost/context/continuation.hpp>
#include <shards/log/log.hpp>

// ASAN (Address Sanitizer) fiber support
// These functions inform ASAN about stack switching so it can track the correct stack bounds
#ifdef SH_USE_ASAN
#include <sanitizer/asan_interface.h>
extern "C" {
// Called before switching to a new fiber stack
// fake_stack_save: output parameter to save fake stack state (can be nullptr)
// stack_bottom: bottom of the new fiber's stack
// stack_size: size of the new fiber's stack
void __sanitizer_start_switch_fiber(void **fake_stack_save, const void *stack_bottom, size_t stack_size);

// Called after returning from a fiber switch
// fake_stack_save: the value saved by start_switch_fiber
// stack_bottom_old: output parameter for the old stack bottom (can be nullptr)
// stack_size_old: output parameter for the old stack size (can be nullptr)
void __sanitizer_finish_switch_fiber(void *fake_stack_save, const void **stack_bottom_old, size_t *stack_size_old);
}
#endif

// TSAN (Thread Sanitizer) fiber support
// These functions inform TSAN about fiber context switches to prevent false positive race reports
#ifdef SH_USE_TSAN
extern "C" {
void *__tsan_get_current_fiber(void);
void *__tsan_create_fiber(unsigned flags);
void __tsan_destroy_fiber(void *fiber);
void __tsan_switch_to_fiber(void *fiber, unsigned flags);
void __tsan_set_fiber_name(void *fiber, const char *name);
}
#endif

namespace shards {
struct SHStackAllocator {
  size_t size{SH_BASE_STACK_SIZE};
  uint8_t *mem{nullptr};

  boost::context::stack_context allocate() {
    boost::context::stack_context ctx;
    ctx.size = size;
    ctx.sp = mem + size;
#if defined(BOOST_USE_VALGRIND)
    ctx.valgrind_stack_id = VALGRIND_STACK_REGISTER(ctx.sp, mem);
#endif
    return ctx;
  }

  void deallocate(boost::context::stack_context &sctx) {
#if defined(BOOST_USE_VALGRIND)
    VALGRIND_STACK_DEREGISTER(sctx.valgrind_stack_id);
#endif
  }
};

struct Fiber {
private:
  SHStackAllocator allocator;
  std::optional<boost::context::continuation> continuation;
#if SH_DEBUG_CONSISTENT_RESUMER
  std::optional<std::thread::id> consistentResumer;
#if SH_DEBUG_CONSISTENT_RESUMER > 1
  std::list<std::string> creatorStack;
#endif
#endif

#ifdef SH_USE_ASAN
  // ASAN fiber state for stack tracking during context switches
  const void *asan_stack_bottom{nullptr}; // Bottom of fiber's stack
  size_t asan_stack_size{0};              // Size of fiber's stack
  void *asan_init_fake_stack{nullptr};    // Fake stack for init() - accessed by lambda
#endif

#ifdef SH_USE_TSAN
  // TSAN fiber handle for tracking fiber context switches
  void *tsan_fiber{nullptr};
#endif

public:
  Fiber(SHStackAllocator allocator);
  ~Fiber();
  Fiber(const Fiber &) = delete;
  Fiber &operator=(const Fiber &) = delete;
  void init(std::function<void()> fn);
  void resume();
  void suspend();
  operator bool() const;
};
} // namespace shards
#elif defined(__EMSCRIPTEN__) && defined(SHARDS_USE_JSPI)
// JSPI-based fiber implementation (Emscripten without Asyncify)
// Uses JavaScript Promise Integration for suspension with manual stack management

// JS function declarations from shards_fiber.js
extern "C" {
int shardsFiberCreate();
void shardsFiberEnter(int fiberId);
void shardsFiberSuspend(int fiberId);
void shardsFiberResume(int fiberId);
void shardsFiberExit(int fiberId);
void shardsFiberDestroy(int fiberId);
int shardsFiberIsCompleted(int fiberId);
void shardsFiberStartEntry(int fiberId, void (*entryFunc)(void *), void *arg);
int shardsFiberGetCurrent();
}

namespace shards {
struct Fiber {
  int fiberId{-1};
  std::function<void()> func;

  Fiber() = default;
  Fiber(size_t) {} // Stack size ignored - JSPI uses growable stacks

  Fiber(const Fiber &) = delete;
  Fiber &operator=(const Fiber &) = delete;

  ~Fiber() {
    if (fiberId >= 0) {
      shardsFiberDestroy(fiberId);
      fiberId = -1;
    }
  }

  void init(const std::function<void()> &func);
  NO_INLINE void resume();
  NO_INLINE void suspend();

  operator bool() const { return fiberId >= 0 && !shardsFiberIsCompleted(fiberId); }
};
} // namespace shards

#else // __EMSCRIPTEN__ with Asyncify (default)
#include <emscripten/fiber.h>
namespace shards {
struct Fiber {
  size_t stack_size;
  static constexpr int as_stack_size = 32770;

  Fiber() : stack_size(SH_BASE_STACK_SIZE) {}
  Fiber(size_t size) : stack_size(size) {}
  ~Fiber() {
    if (c_stack)
      ::operator delete[](c_stack, std::align_val_t{16});
  }
  void init(const std::function<void()> &func);
  NO_INLINE void resume();
  NO_INLINE void suspend();

  // compatibility with boost
  operator bool() const { return true; }

  emscripten_fiber_t em_fiber;
  emscripten_fiber_t *em_parent_fiber{nullptr};
  std::function<void()> func;
  uint8_t asyncify_stack[as_stack_size];
  uint8_t *c_stack{nullptr};
};
} // namespace shards
#endif // SHARDS_USE_JSPI
#endif // SH_USE_THREAD_FIBER

namespace shards {
using Coroutine = std::optional<Fiber>;
inline void coroutineResume(Coroutine &c) { c->resume(); }
inline void coroutineSuspend(Coroutine &c) { c->suspend(); }
inline bool coroutineValid(const Coroutine &c) { return c && *c; }
} // namespace shards

#endif /* D2E9D440_0C35_4166_9CF4_0902B462A99C */

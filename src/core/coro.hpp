#ifndef D2E9D440_0C35_4166_9CF4_0902B462A99C
#define D2E9D440_0C35_4166_9CF4_0902B462A99C

#include <mutex>
#include <shared_mutex>
#include <optional>
#include <thread>
#include <cassert>
#include <atomic>
#include <condition_variable>
#include <semaphore>
#include <variant>
#include <functional>

// TODO make it into a run-time param
#ifndef NDEBUG
#define SH_BASE_STACK_SIZE 1024 * 1024
#else
#define SH_BASE_STACK_SIZE 128 * 1024
#endif

namespace shards {
// Dedicated-thread coroutine
struct ThreadFiber {
private:
  std::mutex mtx;
  std::condition_variable cv;

  std::atomic_bool finished;
  std::atomic_bool isRunning;

  std::optional<std::thread> thread;

public:
  ThreadFiber() = default;
  ThreadFiber(const ThreadFiber &) = delete;
  ThreadFiber &operator=(const ThreadFiber &) = delete;
  void init(std::function<void()> fn);
  void resume();
  void suspend();
  operator bool() const;

private:
  void switchToCaller();
  void switchToThread();
};
} // namespace shards

#ifndef __EMSCRIPTEN__
#include <boost/context/continuation.hpp>
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

public:
  Fiber(SHStackAllocator allocator);
  Fiber(const Fiber &) = delete;
  Fiber &operator=(const Fiber &) = delete;
  void init(std::function<void()> fn);
  void resume();
  void suspend();
  operator bool() const;
};
} // namespace shards
#else
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
  NO_INLINE void yield();

  // compatibility with boost
  operator bool() const { return true; }

  emscripten_fiber_t em_fiber;
  emscripten_fiber_t *em_parent_fiber{nullptr};
  std::function<void()> func;
  uint8_t asyncify_stack[as_stack_size];
  uint8_t *c_stack{nullptr};
};
} // namespace shards
#endif

namespace shards {
struct NullFiber {
  operator bool() const { return false; }
  void resume() {}
  void suspend() {}
};
typedef std::variant<NullFiber, ThreadFiber, Fiber> Coroutine;
inline void coroutineSuspend(Coroutine &c) {
  std::visit([&](auto &inner) { inner.suspend(); }, c);
}
inline void coroutineResume(Coroutine &c) {
  std::visit([&](auto &inner) { inner.resume(); }, c);
}
inline bool coroutineValid(const Coroutine &c) {
  return std::visit([&](const auto &inner) { return (bool)inner; }, c);
}
} // namespace shards

#endif /* D2E9D440_0C35_4166_9CF4_0902B462A99C */

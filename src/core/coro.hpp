#ifndef D2E9D440_0C35_4166_9CF4_0902B462A99C
#define D2E9D440_0C35_4166_9CF4_0902B462A99C

#ifndef __EMSCRIPTEN__
#include <boost/context/continuation.hpp>
#endif

#define SHARDS_HEAVY_CORO
#ifdef SHARDS_HEAVY_CORO
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <thread>
#include <cassert>
#include <atomic>
#include <condition_variable>

// Dedicated-thread coroutine
struct SHHeavyCoro {
private:
  std::mutex mtx;
  std::condition_variable cv;

  std::mutex mtx1;
  std::condition_variable cv1;

  std::atomic_bool finished;

  std::optional<std::thread> thread;

public:
  SHHeavyCoro() = default;
  void init(std::function<void()> fn);
  void resume();
  void suspend();
  operator bool() const;
};
typedef SHHeavyCoro SHCoro;
#elif !defined(__EMSCRIPTEN__)
// For coroutines/context switches
typedef boost::context::continuation SHCoro;

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

#else
#include <emscripten/fiber.h>
struct SHCoro {
  size_t stack_size;
  static constexpr int as_stack_size = 32770;

  SHCoro() : stack_size(SH_BASE_STACK_SIZE) {}
  SHCoro(size_t size) : stack_size(size) {}
  ~SHCoro() {
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
#endif

#endif /* D2E9D440_0C35_4166_9CF4_0902B462A99C */

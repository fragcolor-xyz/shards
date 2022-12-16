#ifndef F419C89A_4B33_40F5_8442_21873D009676
#define F419C89A_4B33_40F5_8442_21873D009676

#include <taskflow/taskflow.hpp>

namespace gfx {

struct GraphicsExecutor;
struct GraphicsFuture {
  GraphicsExecutor *executor;
  tf::Future<void> future;

  void wait();
};

// Wrapper around tf::Executor to support single-threaded execution of Taskflows
struct GraphicsExecutor {
private:
  tf::Executor executor{getNumWorkersToCreate()};
  friend struct GraphicsFuture;

public:
  template <typename T> GraphicsFuture run(T&& flow) { return GraphicsFuture{this, executor.run(std::forward<T>(flow))}; }

  size_t getNumWorkers() const { return executor.num_workers(); }
  size_t getThisWorkerId() const { return executor.this_worker_id(); }

private:
  static inline size_t getNumWorkersToCreate() {
#if GFX_THREADING_SUPPORT
    constexpr size_t minThreads = 1;
    constexpr size_t maxThreads = 3;
    return std::clamp<size_t>(std::thread::hardware_concurrency(), minThreads, maxThreads);
#else
    return 0;
#endif
  }
};

// Wait for the future to be complete
// This should never be called from worker threads to support single-threaded systems
inline void GraphicsFuture::wait() {
#if GFX_THREADING_SUPPORT
  future.wait();
#else
  using namespace std::chrono_literals;
  executor->executor.loop_until([&]() { return future.wait_for(0ms) == std::future_status::ready; });
#endif
}
} // namespace gfx

#endif /* F419C89A_4B33_40F5_8442_21873D009676 */

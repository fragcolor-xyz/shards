#include "async.hpp"

namespace gfx::detail {

struct WorkerInterface : public tf::WorkerInterface {
  GraphicsExecutor *executor;
  bool abortOnWorkerThread = true;

  WorkerInterface(GraphicsExecutor *executor) : executor(executor) {}
  virtual void scheduler_prologue(tf::Worker &worker) {}
  virtual void scheduler_epilogue(tf::Worker &worker, std::exception_ptr ptr) {
    if (ptr) {
      handleExceptionOnWorker(ptr);
    }
  }

  void handleExceptionOnWorker(std::exception_ptr ptr) {
    if (abortOnWorkerThread)
      throw ptr;
    else
      executor->thrownException = std::move(ptr);
  }
};

size_t GraphicsExecutor::getNumWorkersToCreate() {
#if GFX_THREADING_SUPPORT
    constexpr size_t minThreads = 1;
    const size_t maxThreads = std::thread::hardware_concurrency();
    return std::clamp<size_t>(std::thread::hardware_concurrency(), minThreads, maxThreads);
#else
    return 0;
#endif
  }

std::shared_ptr<tf::WorkerInterface> GraphicsExecutor::createWorkerInterface(GraphicsExecutor *executor) {
  return std::make_shared<WorkerInterface>(executor);
}
} // namespace gfx::detail
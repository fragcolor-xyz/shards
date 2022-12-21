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

std::shared_ptr<tf::WorkerInterface> GraphicsExecutor::createWorkerInterface(GraphicsExecutor *executor) {
  return std::make_shared<WorkerInterface>(executor);
}
} // namespace gfx::detail
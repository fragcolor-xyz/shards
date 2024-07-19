#include "platform.hpp"
#if SH_EMSCRIPTEN
#include "assert.hpp"
#include <emscripten.h>
#include <emscripten/proxying.h>
#include <mutex>
#include <list>
#include <future>

namespace shards {
struct EmMainProxy {
  static EmMainProxy *instance;
  static EmMainProxy &getInstance() {
    shassert(instance && "EmProxy not setup");
    return *instance;
  }

private:
  std::mutex mutex;
  using Lock = std::unique_lock<std::mutex>;

  std::list<std::packaged_task<void()>> tasks;

public:
  void poll() {
    if (mutex.try_lock()) {
      for (auto it = tasks.begin(); it != tasks.end();) {
        (*it)();
        it = tasks.erase(it);
      }
      mutex.unlock();
    }
  }

  template <typename F> std::future<void> queue(F &&callback) {
    Lock l(mutex);
    std::packaged_task<void()> task(std::move(callback));
    std::future<void> future = task.get_future();
    tasks.emplace_back(std::move(task));
    return future;
  }
};
} // namespace shards
#endif
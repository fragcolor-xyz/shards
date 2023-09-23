#include "async.hpp"

#if HAS_ASYNC_SUPPORT
namespace shards {
TidePool &getTidePool() {
  static TidePool tidePool;
  return tidePool;
}

void TidePool::controllerWorker() {
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

    TracyPlot("TidePool Workers", int64_t(_workers.size()));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  for (auto &worker : _workers) {
    worker._running = false;
    if (worker._thread.joinable())
      worker._thread.join();
  }
}
} // namespace shards
#endif

#include <future>
#include <vector>
#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <boost/container/stable_vector.hpp>

template<typename RES>
class LoadBalancingPool {
public:
  LoadBalancingPool(unsigned int maxThreads) : _done(false), _maxThreads(maxThreads) {}

  ~LoadBalancingPool() {
    _done = true;

    for (auto &thread : _threads) {
      thread.join();
    }
  }

  template <typename F, typename... Args> auto submit(F &&f) -> std::future<RES> {
    std::packaged_task<RES()> task(std::forward<F>(f));
    auto &pTask = _tasksMem.emplace_back(std::move(task));

    std::future<RES> future = pTask.get_future();

    _tasks.push(&pTask);

    resize();

    return future;
  }

private:
  void worker_thread() {
    while (!_done) {
      if (_tasks.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      std::packaged_task<RES()> *task;
      _tasks.pop(task);

      (*task)();
    }
  }

  void resize() {
    // cleanup our tasks memory, this will still keep our vector capacity the same!
    _tasksMem.erase(std::remove_if(_tasksMem.begin(), _tasksMem.end(), [](const auto &task) {
      return !task.valid();
    }), _tasksMem.end());

    // now clean up threads
    const auto load = _tasksMem.size();
    auto active_threads = _threads.size();

    if (load >= active_threads) {
      // if the load is greater than the number of active threads, create a new thread
      SHLOG_TRACE("Creating new thread, load: {}, active threads: {}", load, active_threads);
      _threads.emplace_back(&LoadBalancingPool::worker_thread, this);
    } else {
      // if the load is less than the number of active threads, detach a thread
      while (load < active_threads && active_threads > _maxThreads) {
        SHLOG_TRACE("Detaching thread, load: {}, active threads: {}", load, active_threads);
        _threads.back().detach();
        _threads.pop_back();
        active_threads = _threads.size();
      }
    }
  }

  boost::lockfree::queue<std::packaged_task<RES()> *> _tasks{16};
  boost::container::stable_vector<std::packaged_task<RES()>> _tasksMem;

  std::vector<std::thread> _threads;
  std::atomic_bool _done;
  unsigned int _maxThreads;
};

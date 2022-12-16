#ifndef F419C89A_4B33_40F5_8442_21873D009676
#define F419C89A_4B33_40F5_8442_21873D009676

#include <taskflow/taskflow.hpp>
#include "worker_memory.hpp"

namespace gfx::detail {

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
  template <typename T> GraphicsFuture run(T &&flow) { return GraphicsFuture{this, executor.run(std::forward<T>(flow))}; }

  size_t getNumWorkers() const { return executor.num_workers(); }
  size_t getThisWorkerId() const { return executor.this_worker_id(); }

private:
  static inline size_t getNumWorkersToCreate() {
#if GFX_THREADING_SUPPORT
    constexpr size_t minThreads = 1;
    const size_t maxThreads = std::thread::hardware_concurrency() - 1;
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

template <typename T> struct TWorkerThreadData {
  using allocator_type = std::pmr::polymorphic_allocator<>;
  std::vector<T> data;
  GraphicsExecutor &executor;

  TWorkerThreadData() = default;
  TWorkerThreadData(GraphicsExecutor &executor) : executor(executor) { data.resize(executor.getNumWorkers()); }
  template <typename... TArgs> TWorkerThreadData(GraphicsExecutor &executor, TArgs... args) : executor(executor) {
    for (size_t i = 0; i < executor.getNumWorkers(); i++) {
      data.emplace_back(std::forward<TArgs...>(args...));
    }
  }
  TWorkerThreadData(TWorkerThreadData &&) = default;
  TWorkerThreadData &operator=(TWorkerThreadData &&) = default;

  T &get() { return get(executor.getThisWorkerId()); }
  T &get(size_t workerIndex) { return data[workerIndex]; }

  auto begin() { return data.begin(); }
  auto end() { return data.end(); }
  auto cbegin() const { return data.cbegin(); }
  auto cend() const { return data.cend(); }
};

// Provides a structure for collecting data accumulated from multiple workers
// there will be a unique instance of T for each worker, lazily created
// each instance will be assigned it's own worker memory based on the constructor value
template <typename T> struct TWorkerCollector {
  using allocator_type = std::pmr::polymorphic_allocator<>;

  struct Iterator {
    std::optional<T> *base;
    size_t index;
    size_t max;

    Iterator &untilNextNonEmpty() {
       while (index < max && !getOpt().has_value()) {
        index++;
      }
      return *this;
    }
    Iterator &operator++() {
      ++index;
      return untilNextNonEmpty();
    }
    bool operator==(const Iterator &other) const { return index == other.index; }
    std::optional<T> &getOpt() const { return base[index]; }
    T &operator*() { return getOpt().value(); }
  };

private:
  std::optional<T> *perWorker{};
  GraphicsExecutor &executor;
  TWorkerThreadData<WorkerMemory> &memories;

public:
  TWorkerCollector(GraphicsExecutor &executor, TWorkerThreadData<WorkerMemory> &memories, allocator_type allocator)
      : executor(executor), memories(memories) {
    size_t arrayLen = sizeof(std::optional<T>) * executor.getNumWorkers();
    perWorker = reinterpret_cast<std::optional<T> *>(allocator.allocate_bytes(arrayLen, 8));
    memset(perWorker, 0, arrayLen);
  }
  TWorkerCollector(TWorkerCollector &&other, allocator_type allocator)
      : perWorker(other.perWorker), executor(other.executor), memories(other.memories) {
    other.perWorker = nullptr;
    *this = std::move(other);
  }
  TWorkerCollector(TWorkerCollector &&other) : perWorker(other.perWorker), executor(other.executor), memories(other.memories) {
    other.perWorker = nullptr;
  }
  TWorkerCollector &operator=(TWorkerCollector &&other) {
    perWorker = other.perWorker;
    other.perWorker = nullptr;
  }

  T &get() {
    size_t workerId = executor.getThisWorkerId();
    auto &elem = perWorker[workerId];
    if (!elem) {
      if constexpr (std::uses_allocator_v<T, allocator_type>) {
        elem.emplace((allocator_type)memories.get(workerId));
      } else {
        elem.emplace();
      }
    }
    return elem.value();
  }
  operator T &() { return get(); }
  T* operator->() { return &get(); }

  Iterator begin() { return Iterator{perWorker, 0, executor.getNumWorkers()}.untilNextNonEmpty(); }
  Iterator end() { return Iterator{perWorker, executor.getNumWorkers(), executor.getNumWorkers()}; }
};

} // namespace gfx::detail

#endif /* F419C89A_4B33_40F5_8442_21873D009676 */

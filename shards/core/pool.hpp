#ifndef EF63CEF1_3538_4C10_B6F5_75FE1E4D2F44
#define EF63CEF1_3538_4C10_B6F5_75FE1E4D2F44

#include <list>
#include <cassert>
#include <memory>
#include <optional>
#include <type_traits>

namespace shards {

template <typename T> struct PoolItemTraits {
  T newItem() {
    assert("not implemented");
    __builtin_unreachable();
  }
  void release(T &) { assert("not implemented"); }
  bool canRecycle(T &v) {
    assert("not implemented");
    __builtin_unreachable();
  }
  void recycled(T &v) { assert("not implemented"); }
};

// Keeps a pool of output objects
// the objects are assumed to be self ref counted, so destruction is not handled by the pool
template <typename T, typename Traits = PoolItemTraits<T>> struct Pool {
  static inline auto itemTraits = Traits{};
  std::list<T> inUseList;
  std::list<T> freeList;

  ~Pool() {
    for (auto &v : inUseList)
      itemTraits.release(v);
    for (auto &v : freeList)
      itemTraits.release(v);
  }

  // Finds a canditate, based on the largest value returned by the condition that is not INT64_MAX
  template <typename F> std::enable_if_t<std::is_invocable_r_v<int64_t, F, T &>, T *> findItem(F condition) {
    int64_t largestWeight = INT64_MIN;
    typename decltype(freeList)::iterator targetFreeListIt = freeList.end();
    for (auto it = freeList.begin(); it != freeList.end(); ++it) {
      auto &item = *it;
      int64_t weight = condition(item);
      if (weight != INT64_MAX && weight > largestWeight) {
        largestWeight = weight;
        targetFreeListIt = it;
      }
    }

    if (targetFreeListIt != freeList.end()) {
      inUseList.splice(inUseList.end(), freeList, targetFreeListIt);
      return &*targetFreeListIt;
    }

    return nullptr;
  }

  T &newValue(auto init, auto condition) {
    auto v = findItem(condition);
    if (!v) {
      auto &result = inUseList.emplace_back(itemTraits.newItem());
      init(result);
      return result;
    }
    return *v;
  }

  T &newValue(auto init) {
    return newValue(init, [](T &v) -> int64_t { return 0; });
  }

  T &newValue() {
    return newValue([](T &v) {}, [](T &v) -> int64_t { return 0; });
  }

  void clear() {
    for (auto &v : inUseList)
      itemTraits.release(v);
    inUseList.clear();
    for (auto &v : freeList)
      itemTraits.release(v);
    freeList.clear();
  }

  // Check returned items and return them to the pool once they are no longer referenced
  void recycle() {
    auto it = inUseList.begin();
    while (it != inUseList.end()) {
      if (itemTraits.canRecycle(*it)) {
        auto nextIt = std::next(it);
        itemTraits.recycled(*it);
        freeList.splice(freeList.end(), inUseList, it);
        it = nextIt;
      } else {
        it = ++it;
      }
    }
  }
};

template <typename J> struct PoolItemTraits<std::shared_ptr<J>> {
  std::shared_ptr<J> newItem() { return std::make_shared<J>(); }
  void release(std::shared_ptr<J> &v) { v.reset(); }
  size_t canRecycle(std::shared_ptr<J> &v) { return v.use_count() == 1; }
  void recycled(std::shared_ptr<J> &v) {}
};
} // namespace shards

#endif /* EF63CEF1_3538_4C10_B6F5_75FE1E4D2F44 */

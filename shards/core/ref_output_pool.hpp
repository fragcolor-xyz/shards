#ifndef EF63CEF1_3538_4C10_B6F5_75FE1E4D2F44
#define EF63CEF1_3538_4C10_B6F5_75FE1E4D2F44

#include <list>
#include <cassert>

namespace shards {

template <typename T> struct RefOutputPoolItemTraits {
  T newItem() { assert("not implemented"); }
  void release(T &) { assert("not implemented"); }
  size_t getRefCount(T &v) { assert("not implemented"); }
};

// Keeps a pool of output objects
// the objects are assumed to be self ref counted, so destruction is not handled by the pool
template <typename T> struct RefOutputPool {
  static inline auto itemTraits = RefOutputPoolItemTraits<T>{};
  std::list<T> inUseList;
  std::list<T> freeList;

  ~RefOutputPool() {
    for (auto &v : inUseList)
      itemTraits.release(v);
    for (auto &v : freeList)
      itemTraits.release(v);
  }

  T newValue(auto init = [](T &v) {}) {
    if (freeList.empty()) {
      auto &result = inUseList.emplace_back(itemTraits.newItem());
      init(std::ref(result));
      return result;
    } else {
      auto it = freeList.begin();
      inUseList.splice(inUseList.end(), freeList, it);
      return *it;
    }
  }

  // Check returned items and return them to the pool once they are no longer referenced
  void recycle() {
    auto it = inUseList.begin();
    while (it != inUseList.end()) {
      if (itemTraits.getRefCount(*it) == 1) {
        auto nextIt = std::next(it);
        freeList.splice(freeList.end(), inUseList, it);
        it = nextIt;
      } else {
        it = ++it;
      }
    }
  }
};
} // namespace shards

#endif /* EF63CEF1_3538_4C10_B6F5_75FE1E4D2F44 */

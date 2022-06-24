#ifndef B32DBC90_5468_49DD_AFEA_C1D0865FED52
#define B32DBC90_5468_49DD_AFEA_C1D0865FED52

#include <vector>

namespace gfx {

template <typename T> struct ResizableItemOps {
  size_t getCapacity(T &item) const { return item.getCapacity(); }
  void init(T &item) {}
};

// Keeps a pool of resizable objects
// When an item with a specific size is requested
// it will try to return the smallest pooled item first
template <typename T, typename TOps = ResizableItemOps<T>> struct ResizableItemPool {
  static inline TOps ops;
  std::vector<T> buffers;
  std::vector<size_t> freeList;

  void reset() {
    freeList.clear();
    for (size_t i = 0; i < buffers.size(); i++) {
      freeList.push_back(i);
    }
  }

  T &allocateBufferNew() {
    T &item = buffers.emplace_back();
    ops.init(item);
    return item;
  }

  T &allocateBufferAny() {
    if (freeList.size() > 0) {
      size_t bufferIndex = freeList.back();
      freeList.pop_back();
      return buffers[bufferIndex];
    } else {
      return allocateBufferNew();
    }
  }

  T &allocateBuffer(size_t size) {
    int64_t smallestDelta = INT64_MAX;
    typename decltype(freeList)::iterator targetFreeListIt = freeList.end();
    for (auto it = freeList.begin(); it != freeList.end(); ++it) {
      size_t bufferIndex = *it;
      auto &buffer = buffers[bufferIndex];
      int64_t delta = ops.getCapacity(buffer) - size;
      if (delta >= 0) {
        if (delta < smallestDelta) {
          smallestDelta = delta;
          targetFreeListIt = it;
        }
      }
    }

    if (targetFreeListIt != freeList.end()) {
      auto bufferIndex = *targetFreeListIt;
      freeList.erase(targetFreeListIt);
      return buffers[bufferIndex];
    } else {
      return allocateBufferNew();
    }
  }
};
} // namespace gfx

#endif /* B32DBC90_5468_49DD_AFEA_C1D0865FED52 */

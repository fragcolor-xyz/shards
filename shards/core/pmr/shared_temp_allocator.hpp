#ifndef D612D3F7_FF79_48A6_A6CE_84380A5C085F
#define D612D3F7_FF79_48A6_A6CE_84380A5C085F

#include "temp_allocator.hpp"

namespace shards::pmr {
struct SharedTempAllocatorImpl;

// An automatically scoped temporary allocator that is local to the current thread.
// The memory will be pooled and reused next time this allocator is created.
// It is ref counted so nested usage is allowed
// The memory pool is reset upon instantiation if there were no previous reference
struct SharedTempAllocator {
private:
  SharedTempAllocatorImpl *impl;

public:
  shards::pmr::memory_resource *getAllocator() const;
  operator shards::pmr::memory_resource *() const { return getAllocator(); }

  SharedTempAllocator();
  SharedTempAllocator(SharedTempAllocator &&other) {
    reset();
    impl = other.impl;
    other.impl = nullptr;
  }
  ~SharedTempAllocator();
  SharedTempAllocator(const SharedTempAllocator &) = delete;
  SharedTempAllocator &operator=(const SharedTempAllocator &) = delete;

  void reset();
};
} // namespace shards::pmr

#endif /* D612D3F7_FF79_48A6_A6CE_84380A5C085F */

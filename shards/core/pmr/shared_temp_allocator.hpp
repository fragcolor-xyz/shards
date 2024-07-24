#ifndef D612D3F7_FF79_48A6_A6CE_84380A5C085F
#define D612D3F7_FF79_48A6_A6CE_84380A5C085F

#include "temp_allocator.hpp"

namespace shards::pmr {
struct SharedTempAllocatorImpl;
struct SharedTempAllocator {
private:
  SharedTempAllocatorImpl *impl;
  // shards::pmr::memory_resource *tempAllocator;
  // int refCount{};

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

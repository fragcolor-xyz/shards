#ifndef D091EB18_DFAE_45E4_B017_6040A9F8C103
#define D091EB18_DFAE_45E4_B017_6040A9F8C103

#include <memory_resource>
#include "moving_average.hpp"
#include "math.hpp"

namespace gfx::detail {

// An implementation of memory_resource
// This behaves like monotonic_buffer_resource
//  with the addition that it updates the preallocated memory block based on previous peak usage
struct MonotonicGrowableAllocator : public std::pmr::memory_resource {
  static constexpr size_t Megabyte = 1 << 20;
  static constexpr size_t MinPreallocatedSize = Megabyte * 8;

  MovingAverage<size_t> maxUsage{32};
  size_t totalRequestedBytes{};
  std::vector<uint8_t> preallocatedBlock;

  std::optional<std::pmr::monotonic_buffer_resource> baseAllocator;
  std::pmr::monotonic_buffer_resource *baseAllocatorPtr{};

  MonotonicGrowableAllocator() { reset(); }
  MonotonicGrowableAllocator(MonotonicGrowableAllocator &&) {}

  void reset() {
    maxUsage.add(totalRequestedBytes);
    totalRequestedBytes = 0;
    updatePreallocatedMemoryBlock();
  }

  void updatePreallocatedMemoryBlock() {
    size_t peakUsage = std::max(MinPreallocatedSize, maxUsage.getMax());
    // Add +1MB headroom and align
    size_t targetSize = alignTo<Megabyte>(peakUsage + Megabyte * 1);
    if (targetSize > preallocatedBlock.size()) {
      preallocatedBlock.resize(targetSize);
    }

    baseAllocator.emplace(preallocatedBlock.data(), preallocatedBlock.size(), std::pmr::new_delete_resource());
    baseAllocatorPtr = &baseAllocator.value();
  }

  __attribute__((always_inline)) void *do_allocate(size_t _Bytes, size_t _Align) override {
    totalRequestedBytes += _Bytes;
    return baseAllocatorPtr->allocate(_Bytes, _Align);
  }

  __attribute__((always_inline)) void do_deallocate(void *_Ptr, size_t _Bytes, size_t _Align) override {
    return baseAllocatorPtr->deallocate(_Ptr, _Bytes, _Align);
  }

  __attribute__((always_inline)) bool do_is_equal(const memory_resource &_That) const noexcept override { return &_That == this; }
};

// Thread-local data for graphics workers
struct WorkerMemory {
  using Allocator = std::pmr::polymorphic_allocator<>;

private:
  MonotonicGrowableAllocator memoryResource;
  Allocator allocator;

public:
  WorkerMemory() : allocator(&memoryResource) {}
  WorkerMemory(WorkerMemory &&) : allocator(&memoryResource) {}

  void reset() { memoryResource.reset(); }

  template<typename T>
  operator std::pmr::polymorphic_allocator<T> &() { return reinterpret_cast<std::pmr::polymorphic_allocator<T>&>(allocator); }
  Allocator *operator->() { return &allocator; }

  const MonotonicGrowableAllocator &getMemoryResource() const { return memoryResource; }
};
} // namespace gfx::detail

#endif /* D091EB18_DFAE_45E4_B017_6040A9F8C103 */

#ifndef D091EB18_DFAE_45E4_B017_6040A9F8C103
#define D091EB18_DFAE_45E4_B017_6040A9F8C103

#include "pmr/wrapper.hpp"
#include "moving_average.hpp"
#include "math.hpp"

#include <thread>
#include <optional>
#include <mutex>

// Enable to check for allocation from wrong thread
#ifdef NDEBUG
#define GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD 0
#else
#define GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD 1
#endif

namespace gfx::detail {

// An implementation of memory_resource
// This behaves like monotonic_buffer_resource
//  with the addition that it updates the preallocated memory block based on previous peak usage
struct MonotonicGrowableAllocator : public shards::pmr::memory_resource {
  static constexpr size_t Megabyte = 1 << 20;
  static constexpr size_t MinPreallocatedSize = Megabyte * 8;

  MovingAverage<size_t> maxUsage{32};
  size_t totalRequestedBytes{};
  std::vector<uint8_t> preallocatedBlock;

  std::optional<shards::pmr::monotonic_buffer_resource> baseAllocator;
  shards::pmr::monotonic_buffer_resource *baseAllocatorPtr{};

#if GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD
  std::optional<std::thread::id> boundThread;
  bool autoBindToThread = false;
#endif

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

    baseAllocator.emplace(preallocatedBlock.data(), preallocatedBlock.size(), shards::pmr::new_delete_resource());
    baseAllocatorPtr = &baseAllocator.value();
  }

  void bindToCurrentThread() {
#if GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD
    boundThread.emplace(std::this_thread::get_id());
#endif
  }

  __attribute__((always_inline)) void *do_allocate(size_t _Bytes, size_t _Align) override {
#if GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD
    // Check for allocations from threads other than the owner of this memory pool
    // since these allocators are not thread safe and meant to be used from a single thread
    // this helps find issues
    if (boundThread) {
      auto callerTid = std::this_thread::get_id();
      if (callerTid != boundThread.value())
        throw std::logic_error("Allocation from non-owning thread");
    } else if (autoBindToThread) {
      bindToCurrentThread();
    }
#endif

    totalRequestedBytes += _Bytes;
    return baseAllocatorPtr->allocate(_Bytes, _Align);
  }

  __attribute__((always_inline)) void do_deallocate(void *_Ptr, size_t _Bytes, size_t _Align) override {
    // Using monotonic_buffer_resource, so safe to no-op
  }

  __attribute__((always_inline)) bool do_is_equal(const memory_resource &_That) const noexcept override { return &_That == this; }
};

// Thread-local data for graphics workers
struct WorkerMemory {
  using Allocator = shards::pmr::PolymorphicAllocator<>;

private:
  MonotonicGrowableAllocator memoryResource;
  Allocator allocator;

public:
  WorkerMemory() : allocator(&memoryResource) { initCommon(); }
  WorkerMemory(WorkerMemory &&) : allocator(&memoryResource) { initCommon(); }

  void reset() { memoryResource.reset(); }

  template <typename T> operator shards::pmr::PolymorphicAllocator<T> &() {
    return reinterpret_cast<shards::pmr::PolymorphicAllocator<T> &>(allocator);
  }
  template <typename T> operator const shards::pmr::PolymorphicAllocator<T> &() {
    return reinterpret_cast<shards::pmr::PolymorphicAllocator<T> &>(allocator);
  }
  Allocator *operator->() { return &allocator; }

  const MonotonicGrowableAllocator &getMemoryResource() const { return memoryResource; }

private:
  void initCommon() {
#if GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD
    memoryResource.autoBindToThread = true;
#endif
  }
};
} // namespace gfx::detail

#endif /* D091EB18_DFAE_45E4_B017_6040A9F8C103 */

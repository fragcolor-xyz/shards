#ifndef D091EB18_DFAE_45E4_B017_6040A9F8C103
#define D091EB18_DFAE_45E4_B017_6040A9F8C103

#include <shards/core/pmr/wrapper.hpp>
#include "moving_average.hpp"
#include "math.hpp"

#ifdef SH_USE_ASAN
#include <sanitizer/asan_interface.h>
#endif

#include <thread>
#include <optional>
#include <mutex>
#include <stdint.h>

// Enable to check for allocation from wrong thread
#ifdef NDEBUG
#define GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD 0
#else
#define GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD 0
#endif

namespace gfx::detail {

// An implementation of memory_resource
// This behaves like monotonic_buffer_resource
//  with the addition that it updates the preallocated memory block based on previous peak usage
struct MonotonicGrowableAllocator final : public shards::pmr::memory_resource {
  static constexpr size_t Megabyte = 1 << 20;
  static constexpr size_t DefaultMinPreallocatedSize = Megabyte * 8;

  MovingAverage<size_t> maxUsage{32};
  size_t totalRequestedBytes{};
  std::vector<uint8_t> preallocatedBlock;

  std::optional<shards::pmr::monotonic_buffer_resource> baseAllocator;
  shards::pmr::monotonic_buffer_resource *baseAllocatorPtr{};

  size_t minPreallocatedSize;

#if GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD
  std::optional<std::thread::id> boundThread;
  bool autoBindToThread = false;
#endif

  MonotonicGrowableAllocator(size_t minPreallocatedSize = DefaultMinPreallocatedSize) : minPreallocatedSize(minPreallocatedSize) {
    reset();
  }
  MonotonicGrowableAllocator(MonotonicGrowableAllocator &&other) : minPreallocatedSize(other.minPreallocatedSize) {}

  void reset() {
    maxUsage.add(totalRequestedBytes);
    totalRequestedBytes = 0;
    updatePreallocatedMemoryBlock();
  }

  void updatePreallocatedMemoryBlock() {
#if SH_USE_ASAN
    if (!preallocatedBlock.empty()) {
      ASAN_UNPOISON_MEMORY_REGION(preallocatedBlock.data(), preallocatedBlock.size());
    }
#endif

    size_t peakUsage = std::max(minPreallocatedSize, maxUsage.getMax());
    // Add +1MB headroom and align
    size_t targetSize = alignTo<Megabyte>(peakUsage + Megabyte * 1);
    if (targetSize > preallocatedBlock.size()) {
      preallocatedBlock.resize(targetSize);
    }

    baseAllocator.emplace(preallocatedBlock.data(), preallocatedBlock.size(), shards::pmr::new_delete_resource());
    baseAllocatorPtr = &baseAllocator.value();

#ifdef SH_USE_ASAN
    ASAN_POISON_MEMORY_REGION(preallocatedBlock.data(), preallocatedBlock.size());
#endif
  }

  void bindToCurrentThread() {
#if GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD
    boundThread.emplace(std::this_thread::get_id());
#endif
  }

  inline void *do_allocate(size_t _Bytes, size_t _Align) override {
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
#if SH_USE_ASAN
    _Align = std::max<size_t>(8, _Align);
    void *ptr = baseAllocatorPtr->allocate(_Bytes, _Align);
    ASAN_UNPOISON_MEMORY_REGION(ptr, _Bytes);
    return ptr;
#else
    return baseAllocatorPtr->allocate(_Bytes, _Align);
#endif
  }

  __attribute__((always_inline)) void do_deallocate(void *_Ptr, size_t _Bytes, size_t _Align) override {
#if SH_USE_ASAN
    ASAN_POISON_MEMORY_REGION(_Ptr, _Bytes);
#endif
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
  WorkerMemory(size_t minPreallocatedSize = MonotonicGrowableAllocator::DefaultMinPreallocatedSize)
      : memoryResource(minPreallocatedSize), allocator(&memoryResource) {
    initCommon();
  }
  WorkerMemory(WorkerMemory &&other) : memoryResource(other.memoryResource.minPreallocatedSize), allocator(&memoryResource) {
    initCommon();
  }

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

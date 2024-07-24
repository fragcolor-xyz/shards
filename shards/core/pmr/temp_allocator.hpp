#ifndef F526B4B0_47CF_4C94_8389_2012F9C56257
#define F526B4B0_47CF_4C94_8389_2012F9C56257

#include <shards/core/pmr/wrapper.hpp>
#include <tracy/Wrapper.hpp>
#include "../../gfx/moving_average.hpp"
#include "../../gfx/math.hpp"

#include <thread>
#include <optional>
#include <mutex>

// Enable to check for allocation from wrong thread
#ifdef NDEBUG
#define GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD 0
#else
#define GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD 0
#endif

namespace shards::pmr {

// An implementation of memory_resource
// This behaves like monotonic_buffer_resource
//  with the addition that it updates the preallocated memory block based on previous peak usage
struct TempAllocator : public shards::pmr::memory_resource {
  static constexpr size_t Megabyte = 1 << 20;
  static constexpr size_t Kilobyte = 1 << 10;
  static constexpr size_t MinPreallocatedSize = Kilobyte * 8;
  static constexpr size_t Headroom = Kilobyte * 4;
  static constexpr size_t Alignment = MinPreallocatedSize;

  gfx::MovingAverage<size_t> maxUsage{32};
  size_t totalRequestedBytes{};
  std::vector<uint8_t> preallocatedBlock;

  std::optional<shards::pmr::monotonic_buffer_resource> baseAllocator;
  shards::pmr::monotonic_buffer_resource *baseAllocatorPtr{};

#if GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD
  std::optional<std::thread::id> boundThread;
  bool autoBindToThread = false;
#endif

  TempAllocator(size_t minSize = MinPreallocatedSize) {
    preallocatedBlock.resize(minSize);
    reset();
  }
  TempAllocator(TempAllocator &&) {}

  void reset() {
    ZoneScoped;

    maxUsage.add(totalRequestedBytes);
    totalRequestedBytes = 0;
    updatePreallocatedMemoryBlock();
  }

  void updatePreallocatedMemoryBlock() {
    size_t peakUsage = maxUsage.getMax();
    size_t targetSize = gfx::alignTo<Alignment>(peakUsage + Headroom);
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
} // namespace shards::pmr

#endif /* F526B4B0_47CF_4C94_8389_2012F9C56257 */

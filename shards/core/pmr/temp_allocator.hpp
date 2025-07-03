#ifndef F526B4B0_47CF_4C94_8389_2012F9C56257
#define F526B4B0_47CF_4C94_8389_2012F9C56257

#include <shards/core/pmr/wrapper.hpp>
#include <shards/core/assert.hpp>
#include <tracy/Wrapper.hpp>
#include "../../gfx/moving_average.hpp"
#include "../../gfx/math.hpp"

#ifdef SH_USE_ASAN
#include <sanitizer/asan_interface.h>
#endif

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
struct TempAllocator final : public shards::pmr::memory_resource {
  static constexpr size_t Megabyte = 1 << 20;
  static constexpr size_t Kilobyte = 1 << 10;
  static constexpr size_t MinPreallocatedSize = Kilobyte * 8;
  static constexpr size_t Headroom = Kilobyte * 4;
  static constexpr size_t Alignment = MinPreallocatedSize;

  gfx::MovingAverage<size_t> maxUsage{32};
  size_t totalRequestedBytes{};
  std::vector<uint8_t> preallocatedBlock;

private:
  std::optional<shards::pmr::monotonic_buffer_resource> baseAllocator;

#if GFX_CHECK_ALLOCATION_FROM_BOUND_THREAD
  std::optional<std::thread::id> boundThread;
  bool autoBindToThread = false;
#endif

public:
  TempAllocator(size_t minSize = MinPreallocatedSize) {
    preallocatedBlock.resize(minSize);
    reset();
  }

  TempAllocator(TempAllocator &&other) {
    shassert(other.totalRequestedBytes == 0 && "Cannot move TempAllocator with pending allocations");
    preallocatedBlock = std::move(other.preallocatedBlock);
    reset();
  }

  void reset() {
    ZoneScoped;

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

    size_t peakUsage = maxUsage.getMax();
    size_t targetSize = gfx::alignTo<Alignment>(peakUsage + Headroom);
    if (targetSize > preallocatedBlock.size()) {
      preallocatedBlock.resize(targetSize);
    }

    baseAllocator.emplace(preallocatedBlock.data(), preallocatedBlock.size(), shards::pmr::new_delete_resource());

#ifdef SH_USE_ASAN
    ASAN_POISON_MEMORY_REGION(preallocatedBlock.data(), preallocatedBlock.size());
#endif
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

#if SH_USE_ASAN
    _Align = std::max<size_t>(8, _Align);
#endif

    void *alignedPtr;
    if (_Align > boost::container::pmr::memory_resource::max_align) {
      // When we need a higher alignment than default, we need to allocate extra
      // to ensure we can align the pointer without overflowing
      size_t extraSpace = _Align - 1;
      size_t sizeAllocated = _Bytes + extraSpace;
      totalRequestedBytes += sizeAllocated;
      char *p = static_cast<char *>(baseAllocator->allocate(sizeAllocated, boost::container::pmr::memory_resource::max_align));

      // Calculate aligned pointer within our allocated block
      alignedPtr = reinterpret_cast<void *>(gfx::alignTo(reinterpret_cast<size_t>(p), _Align));

      // Ensure we didn't overflow our allocation
      shassert(static_cast<char *>(alignedPtr) + _Bytes <= p + sizeAllocated);

    } else {
      totalRequestedBytes += _Bytes;
      alignedPtr = baseAllocator->allocate(_Bytes, _Align);
    }

#if SH_USE_ASAN
    ASAN_UNPOISON_MEMORY_REGION(alignedPtr, _Bytes);
    __asan_update_allocation_context(alignedPtr);
#endif
    return alignedPtr;
  }

  __attribute__((always_inline)) void do_deallocate(void *_Ptr, size_t _Bytes, size_t _Align) override {
#if SH_USE_ASAN
    ASAN_POISON_MEMORY_REGION(_Ptr, _Bytes);
#endif
    // Using monotonic_buffer_resource, so safe to no-op
  }

  __attribute__((always_inline)) bool do_is_equal(const memory_resource &_That) const noexcept override { return &_That == this; }
};
} // namespace shards::pmr

#endif /* F526B4B0_47CF_4C94_8389_2012F9C56257 */

// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2024 Fragcolor Pte. Ltd.

// Minimal PMR (Polymorphic Memory Resource) implementation for freestanding/bare-metal targets
// This provides basic memory resource types without OS dependencies

#ifndef SHARDS_PMR_FREESTANDING_HPP
#define SHARDS_PMR_FREESTANDING_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

#if SH_FREERTOS
#include <FreeRTOS.h>
// FreeRTOS pvPortMalloc/vPortFree
extern "C" void *pvPortMalloc(size_t xSize);
extern "C" void vPortFree(void *pv);
#endif

namespace shards::pmr {

// Base memory_resource interface (matches std::pmr::memory_resource)
class memory_resource {
public:
  static constexpr size_t max_align = alignof(std::max_align_t);

  memory_resource() = default;
  memory_resource(const memory_resource &) = default;
  virtual ~memory_resource() = default;

  memory_resource &operator=(const memory_resource &) = default;

  void *allocate(size_t bytes, size_t alignment = max_align) { return do_allocate(bytes, alignment); }

  void deallocate(void *p, size_t bytes, size_t alignment = max_align) { do_deallocate(p, bytes, alignment); }

  bool is_equal(const memory_resource &other) const noexcept { return do_is_equal(other); }

protected:
  virtual void *do_allocate(size_t bytes, size_t alignment) = 0;
  virtual void do_deallocate(void *p, size_t bytes, size_t alignment) = 0;
  virtual bool do_is_equal(const memory_resource &other) const noexcept = 0;
};

inline bool operator==(const memory_resource &a, const memory_resource &b) noexcept { return &a == &b || a.is_equal(b); }

inline bool operator!=(const memory_resource &a, const memory_resource &b) noexcept { return !(a == b); }

// new_delete_resource - uses standard new/delete (or FreeRTOS malloc/free)
class new_delete_resource_impl final : public memory_resource {
public:
  void *do_allocate(size_t bytes, size_t alignment) override {
#if SH_FREERTOS
    // FreeRTOS doesn't have aligned allocation, so we over-allocate and align
    if (alignment <= alignof(std::max_align_t)) {
      return pvPortMalloc(bytes);
    }
    // For larger alignments, allocate extra and manually align
    size_t extra = alignment - 1 + sizeof(void *);
    void *raw = pvPortMalloc(bytes + extra);
    if (!raw)
      return nullptr;
    // Calculate aligned address
    void *aligned = reinterpret_cast<void *>((reinterpret_cast<uintptr_t>(raw) + extra) & ~(alignment - 1));
    // Store original pointer before aligned block
    reinterpret_cast<void **>(aligned)[-1] = raw;
    return aligned;
#else
    // Standard aligned allocation
    return ::operator new(bytes, std::align_val_t{alignment});
#endif
  }

  void do_deallocate(void *p, size_t bytes, size_t alignment) override {
#if SH_FREERTOS
    if (alignment <= alignof(std::max_align_t)) {
      vPortFree(p);
    } else {
      // Retrieve original pointer stored before aligned block
      void *raw = reinterpret_cast<void **>(p)[-1];
      vPortFree(raw);
    }
#else
    ::operator delete(p, std::align_val_t{alignment});
#endif
  }

  bool do_is_equal(const memory_resource &other) const noexcept override {
    // Without RTTI, compare by pointer (singleton)
    return this == &other;
  }
};

inline memory_resource *new_delete_resource() noexcept {
  static new_delete_resource_impl instance;
  return &instance;
}

// null_memory_resource - always fails allocation
class null_memory_resource_impl final : public memory_resource {
public:
  void *do_allocate(size_t, size_t) override {
#if __cpp_exceptions
    throw std::bad_alloc();
#else
    return nullptr;
#endif
  }

  void do_deallocate(void *, size_t, size_t) override {}

  bool do_is_equal(const memory_resource &other) const noexcept override {
    // Without RTTI, compare by pointer (singleton)
    return this == &other;
  }
};

inline memory_resource *null_memory_resource() noexcept {
  static null_memory_resource_impl instance;
  return &instance;
}

// monotonic_buffer_resource - bump allocator, no individual deallocation
class monotonic_buffer_resource final : public memory_resource {
  void *buffer_;
  size_t buffer_size_;
  size_t offset_;
  memory_resource *upstream_;

public:
  monotonic_buffer_resource() : buffer_(nullptr), buffer_size_(0), offset_(0), upstream_(new_delete_resource()) {}

  monotonic_buffer_resource(void *buffer, size_t size, memory_resource *upstream = new_delete_resource())
      : buffer_(buffer), buffer_size_(size), offset_(0), upstream_(upstream) {}

  monotonic_buffer_resource(size_t initial_size, memory_resource *upstream = new_delete_resource())
      : buffer_(upstream->allocate(initial_size)), buffer_size_(initial_size), offset_(0), upstream_(upstream) {}

  ~monotonic_buffer_resource() override { release(); }

  monotonic_buffer_resource(const monotonic_buffer_resource &) = delete;
  monotonic_buffer_resource &operator=(const monotonic_buffer_resource &) = delete;

  void release() {
    // Note: We don't track/free individual allocations in monotonic mode
    // The upstream only gets called for initial buffer or overflow
    offset_ = 0;
  }

  memory_resource *upstream_resource() const { return upstream_; }

protected:
  void *do_allocate(size_t bytes, size_t alignment) override {
    // Align current offset
    size_t aligned_offset = (offset_ + alignment - 1) & ~(alignment - 1);

    if (aligned_offset + bytes <= buffer_size_) {
      void *result = static_cast<char *>(buffer_) + aligned_offset;
      offset_ = aligned_offset + bytes;
      return result;
    }

    // Overflow - allocate from upstream
    return upstream_->allocate(bytes, alignment);
  }

  void do_deallocate(void *, size_t, size_t) override {
    // Monotonic allocator: no-op for deallocation
  }

  bool do_is_equal(const memory_resource &other) const noexcept override { return this == &other; }
};

// polymorphic_allocator - wraps a memory_resource
template <typename T> class polymorphic_allocator {
  memory_resource *resource_;

public:
  using value_type = T;

  polymorphic_allocator() noexcept : resource_(new_delete_resource()) {}

  polymorphic_allocator(memory_resource *r) noexcept : resource_(r) {}

  template <typename U> polymorphic_allocator(const polymorphic_allocator<U> &other) noexcept : resource_(other.resource()) {}

  polymorphic_allocator(const polymorphic_allocator &) = default;

  polymorphic_allocator &operator=(const polymorphic_allocator &) = delete;

  T *allocate(size_t n) { return static_cast<T *>(resource_->allocate(n * sizeof(T), alignof(T))); }

  void deallocate(T *p, size_t n) { resource_->deallocate(p, n * sizeof(T), alignof(T)); }

  memory_resource *resource() const { return resource_; }

  template <typename U, typename... Args> void construct(U *p, Args &&...args) { ::new (static_cast<void *>(p)) U(std::forward<Args>(args)...); }

  template <typename U> void destroy(U *p) { p->~U(); }

  polymorphic_allocator select_on_container_copy_construction() const { return polymorphic_allocator(); }

  template <typename U> bool operator==(const polymorphic_allocator<U> &other) const noexcept { return *resource_ == *other.resource(); }

  template <typename U> bool operator!=(const polymorphic_allocator<U> &other) const noexcept { return !(*this == other); }
};

// Common container aliases using pmr allocators
template <typename T> using vector = std::vector<T, polymorphic_allocator<T>>;

} // namespace shards::pmr

#endif // SHARDS_PMR_FREESTANDING_HPP

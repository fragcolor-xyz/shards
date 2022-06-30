#ifndef GFX_RENDERER_TYPES
#define GFX_RENDERER_TYPES

#include "gfx_wgpu.hpp"
#include "resizable_item_pool.hpp"
#include <cassert>
#include <vector>

namespace gfx {

// Wraps an object that is swapped per frame for double+ buffered rendering
template <typename TInner, size_t MaxSize> struct Swappable {
  TInner elems[MaxSize];
  TInner &get(size_t frameNumber) {
    assert(frameNumber <= MaxSize);
    return elems[frameNumber];
  }
  TInner &operator()(size_t frameNumber) { return get(frameNumber); }
};

// Wraps a WGPUBuffer with resize operations allowing buffer reuse
struct DynamicWGPUBuffer {
private:
  size_t capacity{};
  WGPUBuffer buffer{};
  WGPUBufferUsageFlags usage{};

public:
  // Resizes the buffer, leaves the data within it undefined
  void resize(WGPUDevice device, size_t size, WGPUBufferUsageFlags usage, const char *label = nullptr) {
    if (this->usage != usage) {
      cleanup();
      this->usage = usage;
    }

    if (size > capacity) {
      cleanup();
      capacity = size;
      init(device, label);
    } else if (size == 0) {
      cleanup();
      capacity = size;
    } else {
      assert(buffer);
    }
  }

  size_t getCapacity() const { return capacity; }
  WGPUBufferUsageFlags getUsageFlags() const { return usage; }
  WGPUBuffer getBuffer() const { return buffer; }
  operator bool() const { return capacity > 0; }
  operator WGPUBuffer() const { return getBuffer(); }

  DynamicWGPUBuffer() = default;
  ~DynamicWGPUBuffer() { cleanup(); }
  DynamicWGPUBuffer(const DynamicWGPUBuffer &other) = delete;
  DynamicWGPUBuffer &operator=(const DynamicWGPUBuffer &other) = delete;
  DynamicWGPUBuffer(DynamicWGPUBuffer &&other) : capacity(other.capacity), buffer(other.buffer), usage(other.usage) {
    other.buffer = nullptr;
    other.capacity = 0;
  }
  DynamicWGPUBuffer &operator=(DynamicWGPUBuffer &&other) {
    capacity = other.capacity;
    buffer = other.buffer;
    usage = other.usage;
    other.buffer = nullptr;
    other.capacity = 0;
    return *this;
  }

private:
  void cleanup() {
    if (buffer) {
      wgpuBufferRelease(buffer);
      buffer = nullptr;
    }
    capacity = 0;
  }

  void init(WGPUDevice device, const char *label) {
    WGPUBufferDescriptor desc{};
    desc.size = capacity;
    desc.usage = usage;
    desc.label = label;
    buffer = wgpuDeviceCreateBuffer(device, &desc);
  }
};

typedef ResizableItemPool<DynamicWGPUBuffer> DynamicWGPUBufferPool;

} // namespace gfx

#endif // GFX_RENDERER_TYPES

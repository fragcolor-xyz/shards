#ifndef CD738B07_92E4_4A3C_A3BA_674D87C141CF
#define CD738B07_92E4_4A3C_A3BA_674D87C141CF

#include "gfx_wgpu.hpp"
#include "sized_item_pool.hpp"
#include "wgpu_handle.hpp"
#include "../core/assert.hpp"
#include <vector>

namespace gfx::detail {

// Wraps a WGPUBuffer with resize operations allowing buffer reuse
struct DynamicWGPUBuffer {
private:
  size_t capacity{};
  WgpuHandle<WGPUBuffer> buffer;
  WGPUBufferUsageFlags usage{};

public:
  // Resizes the buffer, leaves the data within it undefined
  void resize(WGPUDevice device, size_t size, WGPUBufferUsageFlags usage, const char *label = nullptr) {
    if (this->usage != usage) {
      cleanup();
      this->usage = usage;
    }

    if (size > capacity || !buffer) {
      cleanup();
      capacity = size;
      init(device, label);
    } else if (size == 0) {
      cleanup();
      capacity = size;
    } else {
      shassert(buffer);
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
  DynamicWGPUBuffer(DynamicWGPUBuffer &&other) = default;
  DynamicWGPUBuffer &operator=(DynamicWGPUBuffer &&other) = default;

private:
  void cleanup() {
    buffer.release();
    capacity = 0;
  }

  void init(WGPUDevice device, const char *label) {
    WGPUBufferDescriptor desc{};
    desc.size = capacity;
    desc.usage = usage;
    desc.label = label;
    buffer.reset(wgpuDeviceCreateBuffer(device, &desc));
  }
};

} // namespace gfx::detail

#endif /* CD738B07_92E4_4A3C_A3BA_674D87C141CF */

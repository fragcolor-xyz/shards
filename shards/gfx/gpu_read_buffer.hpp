#ifndef C2931EB3_88AD_45EF_98E1_59C895BD3537
#define C2931EB3_88AD_45EF_98E1_59C895BD3537

#include <vector>
#include <memory>
#include "texture.hpp"

namespace gfx {

// Represents a CPU-side memory block that can receive
// some data copied from the gpu asynchronously
struct GpuTextureReadBuffer {
  std::vector<uint8_t> data;
  WGPUTextureFormat pixelFormat{};
  size_t stride{};
  int2 size;
};
using GpuTextureReadBufferPtr = std::shared_ptr<GpuTextureReadBuffer>;

inline GpuTextureReadBufferPtr makeGpuTextureReadBuffer() { return std::make_shared<GpuTextureReadBuffer>(); }

// Represents a CPU-side memory block that can receive
// some data copied from the gpu asynchronously
struct GpuReadBuffer {
  std::vector<uint8_t> data;
  size_t dynamicLength;
};
using GpuReadBufferPtr = std::shared_ptr<GpuReadBuffer>;

inline GpuReadBufferPtr makeGpuReadBuffer() { return std::make_shared<GpuReadBuffer>(); }

} // namespace gfx

#endif /* C2931EB3_88AD_45EF_98E1_59C895BD3537 */

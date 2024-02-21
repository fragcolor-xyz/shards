#ifndef A404C804_6AE3_4409_ADA8_974AFF55A415
#define A404C804_6AE3_4409_ADA8_974AFF55A415

#include "gfx_wgpu.hpp"

namespace gfx {

template <typename TH> struct WgpuRelease {};

#define IMPL_RELEASE(_type, _f)             \
  template <> struct WgpuRelease<_type> {   \
    static void release(_type h) { _f(h); } \
  };

IMPL_RELEASE(WGPUTexture, wgpuTextureRelease);
IMPL_RELEASE(WGPUTextureView, wgpuTextureViewRelease);
IMPL_RELEASE(WGPUSampler, wgpuSamplerRelease);
IMPL_RELEASE(WGPUBuffer, wgpuBufferRelease);
IMPL_RELEASE(WGPUBindGroup, wgpuBindGroupRelease);
IMPL_RELEASE(WGPUBindGroupLayout, wgpuBindGroupLayoutRelease);
IMPL_RELEASE(WGPURenderPassEncoder, wgpuRenderPassEncoderRelease);
IMPL_RELEASE(WGPUCommandEncoder, wgpuCommandEncoderRelease);
IMPL_RELEASE(WGPUCommandBuffer, wgpuCommandBufferRelease);
IMPL_RELEASE(WGPURenderPipeline, wgpuRenderPipelineRelease);
IMPL_RELEASE(WGPUComputePipeline, wgpuComputePipelineRelease);
IMPL_RELEASE(WGPUPipelineLayout, wgpuPipelineLayoutRelease);
IMPL_RELEASE(WGPUShaderModule, wgpuShaderModuleRelease);

#undef IMPL_RELEASE

// Keeps ownership of any wgpu handle and
//   calls release when it goes out of scope
template <typename T> struct WgpuHandle {
  T handle{};

  WgpuHandle() : handle(nullptr){};
  explicit WgpuHandle(const T &handle) : handle(handle) {}

  ~WgpuHandle() { release(); }

  WgpuHandle(WgpuHandle &&other) {
    handle = other.handle;
    other.handle = nullptr;
  }

  WgpuHandle &operator=(WgpuHandle &&other) {
    release();
    handle = other.handle;
    other.handle = nullptr;
    return *this;
  }

  void reset(const T &handle) {
    release();
    this->handle = handle;
  }

  void release() {
    if (handle) {
      WgpuRelease<T>::release(handle);
      handle = nullptr;
    }
  }

  operator T() const { return handle; }
};

template <typename T> inline WgpuHandle<T> toWgpuHandle(const T &handle) { return WgpuHandle<T>(handle); }
} // namespace gfx

#endif /* A404C804_6AE3_4409_ADA8_974AFF55A415 */
